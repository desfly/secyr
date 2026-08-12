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

}  // namespace

esp_err_t AccessHttp::register_handlers(httpd_handle_t server, AccessControl* access, AccessNvsStore* store) {
    if (server == nullptr || access == nullptr || store == nullptr) return ESP_ERR_INVALID_ARG;
    access_ = access;
    store_ = store;
    const httpd_uri_t route{
        .uri = "/api/v1/access/users",
        .method = HTTP_POST,
        .handler = &AccessHttp::users_post,
        .user_ctx = this,
    };
    return httpd_register_uri_handler(server, &route);
}

esp_err_t AccessHttp::users_post(httpd_req_t* request) {
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_users(request);
}

esp_err_t AccessHttp::handle_users(httpd_req_t* request) {
    if (request->content_len == 0 || request->content_len > 768) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }

    std::string body(request->content_len, '\0');
    const auto received = httpd_req_recv(request, body.data(), body.size());
    if (received <= 0) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"read_failed\"}");
    }
    body.resize(static_cast<std::size_t>(received));

    std::string actor;
    std::string credential;
    std::string action;
    if (!parse_json_string(body, "actor", actor) || !parse_json_string(body, "credential", credential) ||
        !parse_json_string(body, "action", action)) {
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
    if (!role_valid || id.size() > 23U || name.size() > 31U || pin.size() < 4U || pin.size() > 12U) {
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
