#include "hg_access_http.hpp"
#include "hg_access_time.hpp"
#include "hg_access_runtime.hpp"
#include "hg_http_session.hpp"

#include "esp_random.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <string_view>

namespace homeguard::idf {
namespace {

AccessHttp* self_from(httpd_req_t* request) {
    return request == nullptr ? nullptr : static_cast<AccessHttp*>(request->user_ctx);
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
            continue;
        }
        if (ch == '\\') { escaped = true; continue; }
        if (ch == '"') return true;
        value.push_back(ch);
    }
    return false;
}

bool parse_bool(const std::string& body, const char* key, bool& value) {
    const auto pos = value_offset(body, key);
    if (pos == std::string::npos) return false;
    if (body.compare(pos, 4, "true") == 0) { value = true; return true; }
    if (body.compare(pos, 5, "false") == 0) { value = false; return true; }
    return false;
}

bool valid_pin(std::string_view pin) {
    if (pin.size() < 4U || pin.size() > 12U) return false;
    for (const char ch : pin) if (ch < '0' || ch > '9') return false;
    return true;
}

void scrub(std::string& secret) {
    std::fill(secret.begin(), secret.end(), '\0');
    secret.clear();
}

std::string json_escape(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 8U);
    for (const char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: if (static_cast<unsigned char>(ch) >= 0x20U) out.push_back(ch); break;
        }
    }
    return out;
}

AccessRole parse_role(std::string_view role, bool& valid) {
    valid = true;
    if (role == "admin") return AccessRole::Admin;
    if (role == "user") return AccessRole::User;
    if (role == "guest") return AccessRole::Guest;
    valid = false;
    return AccessRole::Guest;
}

esp_err_t send_json(httpd_req_t* request, const std::string& body) {
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

bool read_body(httpd_req_t* request, std::size_t limit, std::string& body) {
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

bool read_authorization(httpd_req_t* request, std::string& authorization) {
    authorization.clear();
    if (request == nullptr) return false;
    const auto length = httpd_req_get_hdr_value_len(request, "Authorization");
    if (length == 0U || length > 128U) return false;
    std::array<char, 129> buffer{};
    if (httpd_req_get_hdr_value_str(request, "Authorization", buffer.data(), buffer.size()) != ESP_OK) return false;
    authorization.assign(buffer.data(), length);
    return true;
}

std::string capabilities_json(const AccessControl& access, AccessRole role) {
    const auto allowed = [&](std::string_view command) { return access.role_allows(role, command) ? "true" : "false"; };
    return std::string{"{\"monitor\":true,\"armHome\":"} + allowed("security.arm_home") +
           ",\"armAway\":" + allowed("security.arm_away") +
           ",\"disarm\":" + allowed("security.disarm") +
           ",\"panic\":" + allowed("security.panic") +
           ",\"valves\":" + allowed("valve.open") +
           ",\"networkConfigure\":" + allowed("network.configure") +
           ",\"accessManage\":" + allowed("access.manage") +
           ",\"serviceInvalidate\":" + allowed("system.service.invalidate") + "}";
}

esp_err_t send_rate_limited(httpd_req_t* request, AccessControl& access, std::string_view actor, std::uint64_t now_ms) {
    httpd_resp_set_status(request, "429 Too Many Requests");
    const auto retry = access.authentication_retry_after_ms(actor, now_ms);
    return send_json(request, "{\"ok\":false,\"reason\":\"rate_limited\",\"retryAfterMs\":" + std::to_string(retry) + "}");
}

}  // namespace

esp_err_t AccessHttp::register_handlers(httpd_handle_t server,
                                        AccessControl* access,
                                        AccessNvsStore* store,
                                        bool bootstrap_allowed) {
    if (server == nullptr || access == nullptr || store == nullptr) return ESP_ERR_INVALID_ARG;
    access_ = access;
    store_ = store;
    bootstrap_allowed_ = bootstrap_allowed;
    access_runtime::set_bootstrap_allowed(bootstrap_allowed);

    const httpd_uri_t routes[] = {
        {.uri = "/api/v1/access/state", .method = HTTP_GET, .handler = &AccessHttp::state_get, .user_ctx = this},
        {.uri = "/api/v1/access/users", .method = HTTP_POST, .handler = &AccessHttp::users_post, .user_ctx = this},
        {.uri = "/api/v1/access/login", .method = HTTP_POST, .handler = &AccessHttp::login_post, .user_ctx = this},
        {.uri = "/api/v1/access/logout", .method = HTTP_POST, .handler = &AccessHttp::logout_post, .user_ctx = this},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

esp_err_t AccessHttp::state_get(httpd_req_t* request) {
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_state(request);
}

esp_err_t AccessHttp::users_post(httpd_req_t* request) {
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_users(request);
}

esp_err_t AccessHttp::login_post(httpd_req_t* request) {
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_login(request);
}

esp_err_t AccessHttp::logout_post(httpd_req_t* request) {
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_logout(request);
}

esp_err_t AccessHttp::handle_state(httpd_req_t* request) {
    if (access_ == nullptr) return ESP_FAIL;
    return send_json(request, access_runtime::setup_required(*access_)
        ? "{\"ok\":true,\"state\":\"setup_required\"}"
        : "{\"ok\":true,\"state\":\"login_required\"}");
}

esp_err_t AccessHttp::handle_logout(httpd_req_t* request) {
    std::string authorization;
    if (!read_authorization(request, authorization) || !http_session::revoke(authorization)) {
        scrub(authorization);
        httpd_resp_set_status(request, "401 Unauthorized");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_session\"}");
    }
    scrub(authorization);
    return send_json(request, "{\"ok\":true,\"state\":\"login_required\"}");
}

esp_err_t AccessHttp::handle_login(httpd_req_t* request) {
    if (access_ == nullptr) return ESP_FAIL;
    if (access_runtime::setup_required(*access_)) {
        httpd_resp_set_status(request, "409 Conflict");
        return send_json(request, "{\"ok\":false,\"reason\":\"setup_required\"}");
    }

    std::string body;
    if (!read_body(request, 384U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }

    std::string actor;
    std::string credential;
    if (!parse_json_string(body, "actor", actor) || !parse_json_string(body, "credential", credential) ||
        actor.empty() || actor.size() > 23U || !valid_pin(credential)) {
        scrub(credential); scrub(body);
        httpd_resp_set_status(request, "401 Unauthorized");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_credentials\"}");
    }

    scrub(body);
    const auto now_ms = access_now_ms();
    const auto decision = access_->authenticate(actor, credential, now_ms);
    scrub(credential);
    if (decision == AuditDecision::DeniedRateLimited) return send_rate_limited(request, *access_, actor, now_ms);
    if (decision != AuditDecision::Allowed) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_credentials\"}");
    }

    const auto* user = access_->find_user(actor);
    if (user == nullptr || !user->enabled) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_credentials\"}");
    }

    std::string session_token = http_session::issue(user->id.data(), user->role);
    if (session_token.size() != 64U) {
        scrub(session_token);
        httpd_resp_set_status(request, "503 Service Unavailable");
        return send_json(request, "{\"ok\":false,\"reason\":\"session_unavailable\"}");
    }

    const std::string response = std::string{"{\"ok\":true,\"actor\":\""} + json_escape(user->id.data()) +
        "\",\"name\":\"" + json_escape(user->name.data()) +
        "\",\"role\":\"" + to_string(user->role) +
        "\",\"sessionToken\":\"" + session_token +
        "\",\"capabilities\":" + capabilities_json(*access_, user->role) + "}";
    const auto result = send_json(request, response);
    scrub(session_token);
    return result;
}

esp_err_t AccessHttp::handle_users(httpd_req_t* request) {
    if (access_ == nullptr || store_ == nullptr) return ESP_FAIL;

    std::string body;
    if (!read_body(request, 768U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }

    std::string action;
    if (!parse_json_string(body, "action", action)) {
        scrub(body);
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"action_required\"}");
    }

    if (action == "bootstrap") {
        if (!access_runtime::setup_required(*access_)) {
            bootstrap_allowed_ = false;
            access_runtime::lock_bootstrap();
            scrub(body);
            httpd_resp_set_status(request, "409 Conflict");
            return send_json(request, "{\"ok\":false,\"reason\":\"bootstrap_unavailable\"}");
        }

        std::string id, name, pin;
        if (!parse_json_string(body, "id", id) || !parse_json_string(body, "name", name) ||
            !parse_json_string(body, "pin", pin) || id.empty() || id.size() > 23U ||
            name.empty() || name.size() > 31U || !valid_pin(pin)) {
            scrub(pin); scrub(body);
            httpd_resp_set_status(request, "400 Bad Request");
            return send_json(request, "{\"ok\":false,\"reason\":\"invalid_bootstrap_admin\"}");
        }

        bootstrap_allowed_ = false;
        access_runtime::lock_bootstrap();
        std::array<std::uint8_t, 16> salt{};
        esp_fill_random(salt.data(), salt.size());
        const bool user_set = access_->set_user(id, name, AccessRole::Admin, pin, salt, true);
        scrub(pin); scrub(body);
        if (!user_set) {
            bootstrap_allowed_ = true;
            access_runtime::set_bootstrap_allowed(true);
            httpd_resp_set_status(request, "409 Conflict");
            return send_json(request, "{\"ok\":false,\"reason\":\"bootstrap_failed\"}");
        }

        const auto persist = store_->save(*access_);
        if (persist != ESP_OK) {
            access_->clear_users();
            bootstrap_allowed_ = true;
            access_runtime::set_bootstrap_allowed(true);
            httpd_resp_set_status(request, "500 Internal Server Error");
            return send_json(request, "{\"ok\":false,\"reason\":\"persist_failed\"}");
        }
        http_session::revoke_all();
        return send_json(request, "{\"ok\":true,\"state\":\"login_required\",\"role\":\"admin\"}");
    }

    std::string actor, credential;
    if (!parse_json_string(body, "actor", actor) || !parse_json_string(body, "credential", credential)) {
        scrub(credential); scrub(body);
        httpd_resp_set_status(request, "401 Unauthorized");
        return send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }

    const auto now_ms = access_now_ms();
    const auto decision = access_->authorize(actor, credential, "access.manage", now_ms);
    scrub(credential);
    if (decision == AuditDecision::DeniedRateLimited) {
        scrub(body);
        return send_rate_limited(request, *access_, actor, now_ms);
    }
    if (decision != AuditDecision::Allowed) {
        scrub(body);
        httpd_resp_set_status(request, "403 Forbidden");
        const char* reason = decision == AuditDecision::DeniedRole ? "forbidden_role" : "invalid_credentials";
        return send_json(request, std::string{"{\"ok\":false,\"reason\":\""} + reason + "\"}");
    }

    if (action == "list") {
        scrub(body);
        std::string response = "{\"ok\":true,\"capacity\":8,\"count\":" + std::to_string(access_->user_count()) +
                               ",\"enabledAdmins\":" + std::to_string(access_->enabled_admin_count()) + ",\"users\":[";
        bool first = true;
        for (std::size_t i = 0; i < access_->user_count(); ++i) {
            const auto* user = access_->user_at(i);
            if (user == nullptr) continue;
            if (!first) response.push_back(',');
            first = false;
            response += "{\"id\":\"" + json_escape(user->id.data()) + "\",\"name\":\"" +
                        json_escape(user->name.data()) + "\",\"role\":\"" + to_string(user->role) +
                        "\",\"enabled\":" + (user->enabled ? "true" : "false") + "}";
        }
        response += "]}";
        return send_json(request, response);
    }

    if (action != "set") {
        scrub(body);
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"unknown_action\"}");
    }

    std::string id, name, role_text, pin;
    bool enabled = true;
    if (!parse_json_string(body, "id", id) || !parse_json_string(body, "name", name) ||
        !parse_json_string(body, "role", role_text) || !parse_json_string(body, "pin", pin)) {
        scrub(pin); scrub(body);
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"user_fields_required\"}");
    }
    (void)parse_bool(body, "enabled", enabled);

    bool role_valid = false;
    const auto role = parse_role(role_text, role_valid);
    if (!role_valid || id.empty() || id.size() > 23U || name.empty() || name.size() > 31U || !valid_pin(pin)) {
        scrub(pin); scrub(body);
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_user\"}");
    }

    if (!access_->would_preserve_admin_access(id, role, enabled)) {
        scrub(pin); scrub(body);
        httpd_resp_set_status(request, "409 Conflict");
        return send_json(request, "{\"ok\":false,\"reason\":\"last_admin_required\"}");
    }

    std::unique_ptr<AccessControl> previous_access{new (std::nothrow) AccessControl(*access_)};
    if (!previous_access) {
        scrub(pin); scrub(body);
        httpd_resp_set_status(request, "503 Service Unavailable");
        return send_json(request, "{\"ok\":false,\"reason\":\"rollback_snapshot_unavailable\"}");
    }

    std::array<std::uint8_t, 16> salt{};
    esp_fill_random(salt.data(), salt.size());
    const bool user_set = access_->set_user(id, name, role, pin, salt, enabled);
    scrub(pin); scrub(body);
    if (!user_set) {
        *access_ = *previous_access;
        httpd_resp_set_status(request, "409 Conflict");
        return send_json(request, "{\"ok\":false,\"reason\":\"user_capacity_or_validation\"}");
    }

    const auto persist = store_->save(*access_);
    if (persist != ESP_OK) {
        *access_ = *previous_access;
        httpd_resp_set_status(request, "500 Internal Server Error");
        return send_json(request, "{\"ok\":false,\"reason\":\"persist_failed\"}");
    }

    http_session::revoke_all();
    return send_json(request, "{\"ok\":true,\"enabledAdmins\":" + std::to_string(access_->enabled_admin_count()) + "}");
}

}  // namespace homeguard::idf