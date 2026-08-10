#include "hg_access_admin_http.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace homeguard::idf {
namespace {
constexpr std::size_t kMaxBody = 1024;

bool extract_json_string(const std::string& body, const char* key, std::string& value)
{
    const std::string token = std::string{"\""} + key + "\"";
    const auto key_pos = body.find(token);
    if (key_pos == std::string::npos) return false;
    const auto colon = body.find(':', key_pos + token.size());
    if (colon == std::string::npos) return false;
    const auto first_quote = body.find('"', colon + 1);
    if (first_quote == std::string::npos) return false;
    const auto second_quote = body.find('"', first_quote + 1);
    if (second_quote == std::string::npos) return false;
    value = body.substr(first_quote + 1, second_quote - first_quote - 1);
    return true;
}

bool receive_body(httpd_req_t* request, std::string& body)
{
    if (request == nullptr || request->content_len == 0 || request->content_len > kMaxBody) return false;
    std::string buffer(request->content_len, '\0');
    std::size_t offset = 0;
    while (offset < buffer.size()) {
        const auto received = httpd_req_recv(request, buffer.data() + offset, buffer.size() - offset);
        if (received <= 0) return false;
        offset += static_cast<std::size_t>(received);
    }
    body = std::move(buffer);
    return true;
}

bool authorize_admin(AccessControl& access, const std::string& body)
{
    std::string actor;
    std::string pin;
    if (!extract_json_string(body, "actor", actor) || actor.empty() || actor.size() > 23U ||
        !extract_json_string(body, "actor_pin", pin) || pin.size() < 4U || pin.size() > 12U) {
        return false;
    }
    const auto* user = access.find_user(actor);
    return user != nullptr && user->enabled && user->role == AccessRole::Admin &&
           access.authorize(actor, pin, "access.users.manage") == AuditDecision::Allowed;
}

esp_err_t send_json(httpd_req_t* request, const char* body)
{
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, body, HTTPD_RESP_USE_STRLEN);
}
}

esp_err_t AccessAdminHttp::register_handlers(httpd_handle_t server, AccessControl* access, AccessNvsStore* store)
{
    if (server == nullptr || access == nullptr || store == nullptr) return ESP_ERR_INVALID_ARG;
    access_ = access;
    store_ = store;
    const httpd_uri_t routes[] = {
        {.uri="/api/v1/access/users/enable", .method=HTTP_POST, .handler=&AccessAdminHttp::enable_post, .user_ctx=this},
        {.uri="/api/v1/access/users/disable", .method=HTTP_POST, .handler=&AccessAdminHttp::disable_post, .user_ctx=this},
        {.uri="/api/v1/access/users/delete", .method=HTTP_POST, .handler=&AccessAdminHttp::delete_post, .user_ctx=this},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

AccessAdminHttp* AccessAdminHttp::self_from(httpd_req_t* request)
{
    return request == nullptr ? nullptr : static_cast<AccessAdminHttp*>(request->user_ctx);
}

esp_err_t AccessAdminHttp::enable_post(httpd_req_t* request)
{
    auto* self = self_from(request);
    return self == nullptr ? ESP_ERR_INVALID_ARG : self->set_enabled(request, true);
}

esp_err_t AccessAdminHttp::disable_post(httpd_req_t* request)
{
    auto* self = self_from(request);
    return self == nullptr ? ESP_ERR_INVALID_ARG : self->set_enabled(request, false);
}

esp_err_t AccessAdminHttp::delete_post(httpd_req_t* request)
{
    auto* self = self_from(request);
    return self == nullptr ? ESP_ERR_INVALID_ARG : self->remove(request);
}

esp_err_t AccessAdminHttp::set_enabled(httpd_req_t* request, bool enabled)
{
    if (access_ == nullptr || store_ == nullptr) return ESP_ERR_INVALID_STATE;
    std::string body;
    if (!receive_body(request, body) || !authorize_admin(*access_, body)) {
        return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "admin authentication required");
    }
    std::string id;
    if (!extract_json_string(body, "id", id) || id.empty() || id.size() > 23U) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid user id");
    }
    if (access_->find_user(id) == nullptr) {
        return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "user not found");
    }

    const AccessControl backup = *access_;
    if (!access_->set_user_enabled(id, enabled)) {
        *access_ = backup;
        httpd_resp_set_status(request, "409 Conflict");
        return send_json(request, "{\"accepted\":false,\"code\":\"last_admin_protected\"}");
    }
    if (store_->save(*access_) != ESP_OK) {
        *access_ = backup;
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS save failed");
    }
    return send_json(request, enabled
        ? "{\"accepted\":true,\"enabled\":true}"
        : "{\"accepted\":true,\"enabled\":false}");
}

esp_err_t AccessAdminHttp::remove(httpd_req_t* request)
{
    if (access_ == nullptr || store_ == nullptr) return ESP_ERR_INVALID_STATE;
    std::string body;
    if (!receive_body(request, body) || !authorize_admin(*access_, body)) {
        return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "admin authentication required");
    }
    std::string id;
    if (!extract_json_string(body, "id", id) || id.empty() || id.size() > 23U) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid user id");
    }
    if (access_->find_user(id) == nullptr) {
        return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "user not found");
    }

    const AccessControl backup = *access_;
    if (!access_->remove_user(id)) {
        *access_ = backup;
        httpd_resp_set_status(request, "409 Conflict");
        return send_json(request, "{\"accepted\":false,\"code\":\"last_admin_protected\"}");
    }
    if (store_->save(*access_) != ESP_OK) {
        *access_ = backup;
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS save failed");
    }
    return send_json(request, "{\"accepted\":true,\"deleted\":true}");
}

}  // namespace homeguard::idf
