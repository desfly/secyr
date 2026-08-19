#include "hg_telemetry_session_http.hpp"
#include "hg_access_time.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>

namespace homeguard::idf {
namespace {

TelemetrySessionHttp* self_from(httpd_req_t* request) {
    return static_cast<TelemetrySessionHttp*>(request->user_ctx);
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

bool read_body(httpd_req_t* request, std::size_t limit, std::string& body) {
    if (request == nullptr || request->content_len == 0 || request->content_len > limit) return false;
    body.assign(request->content_len, '\0');
    std::size_t offset = 0;
    while (offset < body.size()) {
        const auto received = httpd_req_recv(request, body.data() + offset, body.size() - offset);
        if (received <= 0) return false;
        offset += static_cast<std::size_t>(received);
    }
    return true;
}

bool valid_pin(const std::string& pin) {
    if (pin.size() < 4U || pin.size() > 12U) return false;
    for (const char ch : pin) if (ch < '0' || ch > '9') return false;
    return true;
}

void scrub(std::string& secret) {
    std::fill(secret.begin(), secret.end(), '\0');
    secret.clear();
}

esp_err_t send_json(httpd_req_t* request, const std::string& body) {
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

}  // namespace

esp_err_t TelemetrySessionHttp::register_handlers(httpd_handle_t server,
                                                   AccessControl* access,
                                                   WebsocketTelemetry* telemetry) {
    if (server == nullptr || access == nullptr || telemetry == nullptr) return ESP_ERR_INVALID_ARG;
    access_ = access;
    telemetry_ = telemetry;
    const httpd_uri_t route{
        .uri = "/api/v1/telemetry/session",
        .method = HTTP_POST,
        .handler = &TelemetrySessionHttp::login_post,
        .user_ctx = this,
    };
    return httpd_register_uri_handler(server, &route);
}

esp_err_t TelemetrySessionHttp::login_post(httpd_req_t* request) {
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_login(request);
}

esp_err_t TelemetrySessionHttp::handle_login(httpd_req_t* request) {
    if (access_ == nullptr || telemetry_ == nullptr) return ESP_FAIL;

    std::string body;
    if (!read_body(request, 384U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }

    std::string actor;
    std::string credential;
    if (!parse_json_string(body, "actor", actor) || !parse_json_string(body, "credential", credential) ||
        actor.empty() || actor.size() > 23U || !valid_pin(credential)) {
        scrub(credential);
        httpd_resp_set_status(request, "401 Unauthorized");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_credentials\"}");
    }

    const auto decision = access_->authenticate(actor, credential, access_now_ms());
    scrub(credential);
    if (decision != AuditDecision::Allowed) {
        httpd_resp_set_status(request, decision == AuditDecision::DeniedRateLimited ? "429 Too Many Requests" : "401 Unauthorized");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_credentials\"}");
    }

    const auto* user = access_->find_user(actor);
    if (user == nullptr || !user->enabled) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_credentials\"}");
    }

    const auto token = telemetry_->issue_session_token();
    if (token.size() < 32U) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return send_json(request, "{\"ok\":false,\"reason\":\"telemetry_unavailable\"}");
    }

    return send_json(request, std::string{"{\"ok\":true,\"telemetryToken\":\""} + token + "\"}");
}

}  // namespace homeguard::idf
