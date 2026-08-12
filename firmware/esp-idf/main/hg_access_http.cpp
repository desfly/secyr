#include "hg_access_http.hpp"

#include "esp_random.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace homeguard::idf {
namespace {

AccessHttp* self_from(httpd_req_t* request) {
    return static_cast<AccessHttp*>(request->user_ctx);
}

bool parse_json_string(const std::string& body, const char* key, std::string& value) {
    const std::string marker = std::string{"\""} + key + "\"";
    auto pos = body.find(marker);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + marker.size());
    if (pos == std::string::npos) return false;
    pos = body.find('"', pos + 1U);
    if (pos == std::string::npos) return false;
    const auto end = body.find('"', pos + 1U);
    if (end == std::string::npos) return false;
    value.assign(body, pos + 1U, end - pos - 1U);
    return true;
}

bool parse_bool(const std::string& body, const char* key, bool& value) {
    const std::string marker = std::string{"\""} + key + "\"";
    auto pos = body.find(marker);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + marker.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t' || body[pos] == '\r' || body[pos] == '\n')) ++pos;
    if (body.compare(pos, 4, "true") == 0) { value = true; return true; }
    if (body.compare(pos, 5, "false") == 0) { value = false; return true; }
    return false;
}

bool valid_pin(std::string_view pin) {
    if (pin.size() < 4U || pin.size() > 12U) return false;
    for (const char ch : pin) {
        if (ch < '0' || ch > '9') return false;
    }
    return true;
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
            default:
                if (static_cast<unsigned char>(ch) >= 0x20U) out.push_back(ch);
                break;
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
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
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

std::string capabilities_json(const AccessControl& access, AccessRole role) {
    const auto allowed = [&](std::string_view command) { return access.role_allows(role, command) ? "true" : "false"; };
    return std::string{"{\"monitor\":true,\"armHome\":"} + allowed("security.arm_home") +
           ",\"armAway\":" + allowed("security.arm_away") +
           ",\"disarm\":" + allowed("security.disarm") +
           ",\"panic\":" + allowed("security.panic") +
           ",\"valves\":" + allowed("valve.open") +
           ",\"networkConfigure\":" + allowed("system.network.configure") +
           ",\"accessManage\":" + allowed("access.manage") +
           ",\"serviceInvalidate\":" + allowed("system.service.invalidate") + "}";
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
    const httpd_uri_t routes[] = {
        {.uri = "/api/v1/access/users", .method = HTTP_POST, .handler = &AccessHttp::users_post, .user_ctx = this},
        {.uri = "/api/v1/access/login", .method = HTTP_POST, .handler = &AccessHttp::login_post, .user_ctx = this},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

esp_err_t AccessHttp::users_post(httpd_req_t* request) {
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_users(request);
}

esp_err_t AccessHttp::login_post(httpd_req_t* request) {
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_login(request);
}

esp_err_t AccessHttp::handle_login(httpd_req_t* request) {
    std::string body;
    if (!read_body(request, 384U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }

    std::string actor;
    std::string credential;
    if (!parse_json_string(body, "actor", actor) || !parse_json_string(body, "credential", credential) ||
        actor.empty() || actor.size() > 23U || !valid_pin(credential)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_credentials\"}");
    }

    const auto decision = access_->authenticate(actor, credential);
    if (decision != AuditDecision::Allowed) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return send_json(request, std::string{"{\"ok\":false,\"reason\":\""} + to_string(decision) + "\"}");
    }

    const auto* user = access_->find_user(actor);
    if (user == nullptr || !user->enabled) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return send_json(request, "{\"ok\":false,\"reason\":\"denied_unknown_user\"}");
    }

    return send_json(request,
        std::string{"{\"ok\":true,\"actor\":\""} + json_escape(user->id.data()) +
        "\",\"name\":\"" + json_escape(user->name.data()) +
        "\",\"role\":\"" + to_string(user->role) +
        "\",\"capabilities\":" + capabilities_json(*access_, user->role) + "}");
}

esp_err_t AccessHttp::handle_users(httpd_req_t* request) {
    std::string body;
    if (!read_body(request, 768U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }

    std::string action;
    if (!parse_json_string(body, "action", action)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"action_required\"}");
    }

    // Bootstrap is available only when boot proved that the access NVS key
    // does not exist. Corrupt or unreadable access storage stays fail-closed.
    if (action == "bootstrap") {
        if (!bootstrap_allowed_) {
            httpd_resp_set_status(request, "409 Conflict");
            return send_json(request, "{\"ok\":false,\"reason\":\"bootstrap_unavailable\"}");
        }
        if (access_->user_count() != 0U) {
            bootstrap_allowed_ = false;
            httpd_resp_set_status(request, "409 Conflict");
            return send_json(request, "{\"ok\":false,\"reason\":\"already_provisioned\"}");
        }

        std::string id;
        std::string name;
        std::string pin;
        if (!parse_json_string(body, "id", id) || !parse_json_string(body, "name", name) ||
            !parse_json_string(body, "pin", pin) || id.empty() || id.size() > 23U ||
            name.empty() || name.size() > 31U || !valid_pin(pin)) {
            httpd_resp_set_status(request, "400 Bad Request");
            return send_json(request, "{\"ok\":false,\"reason\":\"invalid_bootstrap_admin\"}");
        }

        // Close the one-time gate before mutating RAM. If persistence fails,
        // rollback and reopen only this factory commissioning attempt.
        bootstrap_allowed_ = false;
        std::array<std::uint8_t, 16> salt{};
        esp_fill_random(salt.data(), salt.size());
        if (!access_->set_user(id, name, AccessRole::Admin, pin, salt, true)) {
            bootstrap_allowed_ = true;
            httpd_resp_set_status(request, "409 Conflict");
            return send_json(request, "{\"ok\":false,\"reason\":\"bootstrap_failed\"}");
        }

        const auto persist = store_->save(*access_);
        if (persist != ESP_OK) {
            access_->clear_users();
            bootstrap_allowed_ = true;
            httpd_resp_set_status(request, "500 Internal Server Error");
            return send_json(request, "{\"ok\":false,\"reason\":\"persist_failed\"}");
        }
        return send_json(request, "{\"ok\":true,\"role\":\"admin\",\"bootstrap\":true}");
    }

    std::string actor;
    std::string credential;
    if (!parse_json_string(body, "actor", actor) || !parse_json_string(body, "credential", credential)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }

    const auto decision = access_->authorize(actor, credential, "access.manage");
    if (decision != AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return send_json(request, std::string{"{\"ok\":false,\"reason\":\""} + to_string(decision) + "\"}");
    }

    if (action == "list") {
        std::string response = "{\"ok\":true,\"capacity\":8,\"count\":" + std::to_string(access_->user_count()) + ",\"users\":[";
        for (std::size_t i = 0; i < access_->user_count(); ++i) {
            const auto* user = access_->user_at(i);
            if (user == nullptr) continue;
            if (i != 0U) response.push_back(',');
            response += "{\"id\":\"" + json_escape(user->id.data()) + "\",\"name\":\"" +
                        json_escape(user->name.data()) + "\",\"role\":\"" + to_string(user->role) +
                        "\",\"enabled\":" + (user->enabled ? "true" : "false") + "}";
        }
        response += "]}";
        return send_json(request, response);
    }

    if (action != "set") {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"unknown_action\"}");
    }

    std::string id;
    std::string name;
    std::string role_text;
    std::string pin;
    bool enabled = true;
    if (!parse_json_string(body, "id", id) || !parse_json_string(body, "name", name) ||
        !parse_json_string(body, "role", role_text) || !parse_json_string(body, "pin", pin)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"user_fields_required\"}");
    }
    (void)parse_bool(body, "enabled", enabled);

    bool role_valid = false;
    const auto role = parse_role(role_text, role_valid);
    if (!role_valid || id.empty() || id.size() > 23U || name.empty() || name.size() > 31U || !valid_pin(pin)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_user\"}");
    }

    std::array<std::uint8_t, 16> salt{};
    esp_fill_random(salt.data(), salt.size());
    if (!access_->set_user(id, name, role, pin, salt, enabled)) {
        httpd_resp_set_status(request, "409 Conflict");
        return send_json(request, "{\"ok\":false,\"reason\":\"user_capacity_or_validation\"}");
    }

    const auto persist = store_->save(*access_);
    if (persist != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return send_json(request, "{\"ok\":false,\"reason\":\"persist_failed\"}");
    }

    return send_json(request, "{\"ok\":true}");
}

}  // namespace homeguard::idf
