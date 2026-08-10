#include "hg_output_http.hpp"
#include "homeguard/output_command.hpp"
#include "homeguard/physical_output_diagnostics.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>

namespace homeguard::idf {
namespace {
OutputHttp* self_from(httpd_req_t* request) {
    return static_cast<OutputHttp*>(request->user_ctx);
}

bool parse_uint(const std::string& body, const char* key, std::uint16_t& value) {
    const std::string marker = std::string{"\""} + key + "\":";
    const auto pos = body.find(marker);
    if (pos == std::string::npos) return false;
    const auto first = body.data() + pos + marker.size();
    const auto last = body.data() + body.size();
    unsigned parsed{};
    const auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc{} || parsed > 65535U) return false;
    value = static_cast<std::uint16_t>(parsed);
    return true;
}

bool parse_bool(const std::string& body, const char* key, bool& value) {
    const std::string marker = std::string{"\""} + key + "\":";
    const auto pos = body.find(marker);
    if (pos == std::string::npos) return false;
    const auto value_pos = pos + marker.size();
    if (body.compare(value_pos, 4, "true") == 0) { value = true; return true; }
    if (body.compare(value_pos, 5, "false") == 0) { value = false; return true; }
    return false;
}
}

esp_err_t OutputHttp::register_handlers(
    httpd_handle_t server,
    hg::SystemModel* model,
    hg::BootReadinessReport* readiness,
    hg::PhysicalOutputRuntime* physical,
    hg::SystemEventBus* bus)
{
    if (server == nullptr || model == nullptr || readiness == nullptr || physical == nullptr || bus == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    model_ = model;
    readiness_ = readiness;
    physical_ = physical;
    bus_ = bus;
    const httpd_uri_t routes[] = {
        {.uri="/api/v1/system/output-command", .method=HTTP_POST, .handler=&OutputHttp::command_post, .user_ctx=this},
        {.uri="/api/v1/system/output-runtime", .method=HTTP_GET, .handler=&OutputHttp::runtime_get, .user_ctx=this},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

esp_err_t OutputHttp::command_post(httpd_req_t* request) {
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_command(request);
}

esp_err_t OutputHttp::runtime_get(httpd_req_t* request) {
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_runtime(request);
}

esp_err_t OutputHttp::handle_runtime(httpd_req_t* request) const {
    if (physical_ == nullptr || readiness_ == nullptr) return ESP_FAIL;
    const auto diagnostics = hg::make_physical_output_diagnostics(physical_->state(), *readiness_);
    const auto body = hg::physical_output_diagnostics_json(diagnostics);
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

esp_err_t OutputHttp::handle_command(httpd_req_t* request) {
    if (request->content_len == 0 || request->content_len > 256) {
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"invalid_body\"}", -1);
    }

    std::string body(request->content_len, '\0');
    const auto received = httpd_req_recv(request, body.data(), body.size());
    if (received <= 0) {
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"read_failed\"}", -1);
    }
    body.resize(static_cast<std::size_t>(received));

    std::uint16_t output_id{};
    bool active{};
    bool alarm_active{};
    if (!parse_uint(body, "outputId", output_id) || !parse_bool(body, "active", active)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"invalid_command\"}", -1);
    }
    (void)parse_bool(body, "alarmActive", alarm_active);

    const auto result = hg::apply_output_command(
        *model_, *readiness_, {output_id, active, alarm_active, 0});

    if (result.status == hg::OutputCommandStatus::Applied && !physical_->synchronize(*model_, *readiness_)) {
        (void)model_->set_output_active(output_id, false, 0);
        (void)physical_->force_safe();
        httpd_resp_set_status(request, "503 Service Unavailable");
        httpd_resp_set_type(request, "application/json");
        return httpd_resp_send(request,
            "{\"ok\":false,\"reason\":\"physical_output_failure\",\"active\":false}", -1);
    }

    std::string response = std::string{"{\"ok\":"} +
        (result.status == hg::OutputCommandStatus::Applied ? "true" : "false") +
        ",\"status\":\"" + hg::to_string(result.status) +
        "\",\"interlock\":\"" + hg::to_string(result.interlock) +
        "\",\"active\":" + (result.resulting_active ? "true" : "false") + "}";

    if (result.status != hg::OutputCommandStatus::Applied) {
        httpd_resp_set_status(request, "409 Conflict");
    } else if (bus_ != nullptr) {
        bus_->publish({hg::SystemEventType::ConfigChanged, output_id, 0, 0, active ? 5501 : 5500});
    }
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, response.c_str(), static_cast<ssize_t>(response.size()));
}

}  // namespace homeguard::idf
