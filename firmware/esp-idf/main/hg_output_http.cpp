#include "hg_output_http.hpp"
#include "homeguard/output_command.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>

namespace homeguard::idf {
namespace {
OutputHttp* self_from(httpd_req_t* request) { return static_cast<OutputHttp*>(request->user_ctx); }

bool parse_uint(const std::string& body, const char* key, std::uint16_t& value) {
    const std::string marker = std::string{"\""} + key + "\":";
    const auto pos = body.find(marker); if (pos == std::string::npos) return false;
    const auto first = body.data() + pos + marker.size(); const auto last = body.data() + body.size();
    unsigned parsed{}; const auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc{} || parsed > 65535U) return false;
    value = static_cast<std::uint16_t>(parsed); return true;
}

bool parse_bool(const std::string& body, const char* key, bool& value) {
    const std::string marker = std::string{"\""} + key + "\":";
    const auto pos = body.find(marker); if (pos == std::string::npos) return false;
    const auto value_pos = pos + marker.size();
    if (body.compare(value_pos, 4, "true") == 0) { value = true; return true; }
    if (body.compare(value_pos, 5, "false") == 0) { value = false; return true; }
    return false;
}

bool parse_json_string(const std::string& body, const char* key, std::string& value) {
    const std::string marker = std::string{"\""} + key + "\"";
    auto pos = body.find(marker); if (pos == std::string::npos) return false;
    pos = body.find(':', pos + marker.size()); if (pos == std::string::npos) return false;
    pos = body.find('"', pos + 1U); if (pos == std::string::npos) return false;
    const auto end = body.find('"', pos + 1U); if (end == std::string::npos) return false;
    value.assign(body, pos + 1U, end - pos - 1U); return true;
}
}

esp_err_t OutputHttp::register_handlers(httpd_handle_t server, hg::SystemModel* model,
    hg::BootReadinessReport* readiness, hg::PhysicalOutputRuntime* physical, hg::SystemEventBus* bus)
{
    if (server == nullptr || model == nullptr || readiness == nullptr || physical == nullptr || bus == nullptr) return ESP_ERR_INVALID_ARG;
    model_ = model; readiness_ = readiness; physical_ = physical; bus_ = bus;
    const httpd_uri_t route{.uri="/api/v1/system/output-command",.method=HTTP_POST,.handler=&OutputHttp::command_post,.user_ctx=this};
    return httpd_register_uri_handler(server, &route);
}

esp_err_t OutputHttp::command_post(httpd_req_t* request) {
    auto* self = self_from(request); return self == nullptr ? ESP_FAIL : self->handle_command(request);
}

esp_err_t OutputHttp::handle_command(httpd_req_t* request) {
    if (request->content_len == 0 || request->content_len > 384) {
        httpd_resp_set_status(request, "400 Bad Request"); return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"invalid_body\"}", -1);
    }
    std::string body(request->content_len, '\0'); const auto received = httpd_req_recv(request, body.data(), body.size());
    if (received <= 0) { httpd_resp_set_status(request, "400 Bad Request"); return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"read_failed\"}", -1); }
    body.resize(static_cast<std::size_t>(received));

    std::uint16_t output_id{}; bool active{}; bool alarm_active{}; std::string actor; std::string credential;
    if (!parse_uint(body, "outputId", output_id) || !parse_bool(body, "active", active)) {
        httpd_resp_set_status(request, "400 Bad Request"); return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"invalid_command\"}", -1);
    }
    (void)parse_bool(body, "alarmActive", alarm_active);
    if (!parse_json_string(body, "actor", actor) || !parse_json_string(body, "credential", credential)) {
        httpd_resp_set_status(request, "401 Unauthorized"); httpd_resp_set_type(request, "application/json");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"credential_required\"}", -1);
    }
    if (access_control_ == nullptr || output_access_ == nullptr) {
        httpd_resp_set_status(request, "503 Service Unavailable"); httpd_resp_set_type(request, "application/json");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"access_unavailable\"}", -1);
    }

    const auto* output = model_->output(output_id);
    const std::string command = output != nullptr && output->type == hg::ModelOutputType::Valve ? (active ? "valve.open" : "valve.close") : "output.control";
    const auto decision = access_control_->authorize(actor, credential, command);
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden"); httpd_resp_set_type(request, "application/json");
        const std::string response = std::string{"{\"ok\":false,\"reason\":\""} + homeguard::to_string(decision) + "\"}";
        return httpd_resp_send(request, response.c_str(), static_cast<ssize_t>(response.size()));
    }

    const auto* user = access_control_->find_user(actor);
    if (user == nullptr) {
        httpd_resp_set_status(request, "403 Forbidden"); return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"unknown_user\"}", -1);
    }
    if (user->role != homeguard::AccessRole::Admin) {
        const auto* rule = output_access_->rule(actor, output_id);
        const bool allowed = rule != nullptr && rule->visible && (active ? rule->can_on : rule->can_off);
        if (!allowed) {
            httpd_resp_set_status(request, "403 Forbidden"); httpd_resp_set_type(request, "application/json");
            return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"output_matrix_denied\"}", -1);
        }
    }

    const auto result = hg::apply_output_command(*model_, *readiness_, {output_id, active, alarm_active, 0});
    if (result.status == hg::OutputCommandStatus::Applied && !physical_->synchronize(*model_, *readiness_)) {
        (void)model_->set_output_active(output_id, false, 0); (void)physical_->force_safe();
        httpd_resp_set_status(request, "503 Service Unavailable"); httpd_resp_set_type(request, "application/json");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"physical_output_failure\",\"active\":false}", -1);
    }

    std::string response = std::string{"{\"ok\":"} + (result.status == hg::OutputCommandStatus::Applied ? "true" : "false") +
        ",\"status\":\"" + hg::to_string(result.status) + "\",\"interlock\":\"" + hg::to_string(result.interlock) +
        "\",\"active\":" + (result.resulting_active ? "true" : "false") + "}";
    if (result.status != hg::OutputCommandStatus::Applied) httpd_resp_set_status(request, "409 Conflict");
    else if (bus_ != nullptr) bus_->publish({hg::SystemEventType::ConfigChanged, output_id, 0, 0, active ? 5401 : 5400});
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, response.c_str(), static_cast<ssize_t>(response.size()));
}

}  // namespace homeguard::idf
