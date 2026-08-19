#include "hg_output_http.hpp"
#include "hg_request_auth.hpp"
#include "homeguard/output_command.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

namespace homeguard::idf {
namespace {
OutputHttp* self_from(httpd_req_t* request) {
    return static_cast<OutputHttp*>(request->user_ctx);
}

std::size_t value_offset(const std::string& body, const char* key) {
    const std::string marker = std::string{"\""} + key + "\"";
    auto pos = body.find(marker);
    if (pos == std::string::npos) return std::string::npos;
    pos = body.find(':', pos + marker.size());
    if (pos == std::string::npos) return std::string::npos;
    ++pos;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
    return pos;
}

bool parse_uint(const std::string& body, const char* key, std::uint16_t& value) {
    const auto pos = value_offset(body, key);
    if (pos == std::string::npos) return false;
    const auto first = body.data() + pos;
    const auto last = body.data() + body.size();
    unsigned parsed{};
    const auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc{} || result.ptr == first || parsed > 65535U) return false;
    value = static_cast<std::uint16_t>(parsed);
    return true;
}

bool parse_bool(const std::string& body, const char* key, bool& value) {
    const auto pos = value_offset(body, key);
    if (pos == std::string::npos) return false;
    if (body.compare(pos, 4, "true") == 0) { value = true; return true; }
    if (body.compare(pos, 5, "false") == 0) { value = false; return true; }
    return false;
}

bool parse_json_string(const std::string& body, const char* key, std::string& value) {
    auto pos = value_offset(body, key);
    if (pos == std::string::npos || pos >= body.size() || body[pos] != '"') return false;
    ++pos;
    value.clear();
    bool escaped = false;
    for (; pos < body.size(); ++pos) {
        const char ch = body[pos];
        if (escaped) {
            if (ch == '"' || ch == '\\' || ch == '/') value.push_back(ch);
            else if (ch == 'n') value.push_back('\n');
            else if (ch == 'r') value.push_back('\r');
            else if (ch == 't') value.push_back('\t');
            else return false;
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == '"') {
            return true;
        } else {
            value.push_back(ch);
        }
    }
    return false;
}

bool read_request_body(httpd_req_t* request, std::size_t limit, std::string& body) {
    if (request == nullptr || request->content_len == 0 || request->content_len > limit) return false;
    body.assign(request->content_len, '\0');
    std::size_t offset = 0U;
    while (offset < body.size()) {
        const auto received = httpd_req_recv(request, body.data() + offset, body.size() - offset);
        if (received <= 0) return false;
        offset += static_cast<std::size_t>(received);
    }
    return true;
}

void scrub(std::string& secret) {
    std::fill(secret.begin(), secret.end(), '\0');
    secret.clear();
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
    const httpd_uri_t route{
        .uri="/api/v1/system/output-command",
        .method=HTTP_POST,
        .handler=&OutputHttp::command_post,
        .user_ctx=this,
    };
    return httpd_register_uri_handler(server, &route);
}

esp_err_t OutputHttp::command_post(httpd_req_t* request) {
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_command(request);
}

esp_err_t OutputHttp::handle_command(httpd_req_t* request) {
    std::string body;
    if (!read_request_body(request, 384U, body)) {
        scrub(body);
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"invalid_body\"}", -1);
    }

    std::uint16_t output_id{};
    bool active{};
    bool alarm_active{};
    std::string actor;
    std::string credential;
    if (!parse_uint(body, "outputId", output_id) || !parse_bool(body, "active", active)) {
        scrub(body);
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"invalid_command\"}", -1);
    }
    (void)parse_bool(body, "alarmActive", alarm_active);

    if (!parse_json_string(body, "actor", actor) || !parse_json_string(body, "credential", credential)) {
        scrub(credential);
        scrub(body);
        httpd_resp_set_status(request, "401 Unauthorized");
        httpd_resp_set_type(request, "application/json");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"credential_required\"}", -1);
    }
    scrub(body);
    if (access_control_ == nullptr) {
        scrub(credential);
        httpd_resp_set_status(request, "503 Service Unavailable");
        httpd_resp_set_type(request, "application/json");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"access_unavailable\"}", -1);
    }
    if (!request_auth::authenticated_actor(request, *access_control_, actor)) {
        scrub(credential);
        return request_auth::send_login_required(request);
    }

    const auto* output = model_->output(output_id);
    const std::string command = output != nullptr && output->type == hg::ModelOutputType::Valve
        ? (active ? "valve.open" : "valve.close")
        : "output.control";
    const auto decision = access_control_->authorize(actor, credential, command);
    scrub(credential);
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        httpd_resp_set_type(request, "application/json");
        const std::string response = std::string{"{\"ok\":false,\"reason\":\""} +
            homeguard::to_string(decision) + "\"}";
        return httpd_resp_send(request, response.c_str(), static_cast<ssize_t>(response.size()));
    }

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
        bus_->publish({hg::SystemEventType::ConfigChanged, output_id, 0, 0, active ? 5401 : 5400});
    }
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, response.c_str(), static_cast<ssize_t>(response.size()));
}

}  // namespace homeguard::idf
