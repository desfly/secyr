#include "hg_access_bootstrap_http.hpp"

#include "esp_log.h"
#include "esp_random.h"
#include "homeguard/access_store.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "hg_access_bootstrap";
constexpr std::size_t kMaxBody = 4096;

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

std::array<std::uint8_t, 16> make_salt()
{
    std::array<std::uint8_t, 16> salt{};
    for (std::size_t i = 0; i < salt.size(); i += 4) {
        const std::uint32_t value = esp_random();
        salt[i] = static_cast<std::uint8_t>(value >> 24);
        salt[i + 1] = static_cast<std::uint8_t>(value >> 16);
        salt[i + 2] = static_cast<std::uint8_t>(value >> 8);
        salt[i + 3] = static_cast<std::uint8_t>(value);
    }
    return salt;
}

bool parse_role(std::string_view text, AccessRole& role)
{
    if (text == "admin") role = AccessRole::Admin;
    else if (text == "user") role = AccessRole::User;
    else if (text == "guest") role = AccessRole::Guest;
    else return false;
    return true;
}

bool authorize_admin(AccessControl& access, const std::string& body, std::string_view command)
{
    std::string actor;
    std::string pin;
    if (!extract_json_string(body, "actor", actor) || actor.empty() || actor.size() > 23 ||
        !extract_json_string(body, "actor_pin", pin) || pin.size() < 4 || pin.size() > 12) {
        return false;
    }
    const auto* user = access.find_user(actor);
    if (user == nullptr || user->role != AccessRole::Admin) return false;
    return access.authorize(actor, pin, command) == AuditDecision::Allowed;
}

std::string json_escape(std::string_view value)
{
    std::string out;
    out.reserve(value.size());
    for (const char ch : value) {
        if (ch == '"' || ch == '\\') out.push_back('\\');
        if (static_cast<unsigned char>(ch) >= 0x20U) out.push_back(ch);
    }
    return out;
}

std::string image_to_hex(const AccessStoreCodec::Image& image)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(image.size() * 2U);
    for (const auto item : image) {
        const auto byte = std::to_integer<std::uint8_t>(item);
        out.push_back(digits[(byte >> 4U) & 0x0fU]);
        out.push_back(digits[byte & 0x0fU]);
    }
    return out;
}

int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
    if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
    return -1;
}

bool image_from_hex(std::string_view text, AccessStoreCodec::Image& image)
{
    if (text.size() != image.size() * 2U) return false;
    for (std::size_t i = 0; i < image.size(); ++i) {
        const int high = hex_value(text[i * 2U]);
        const int low = hex_value(text[i * 2U + 1U]);
        if (high < 0 || low < 0) return false;
        image[i] = static_cast<std::byte>((high << 4) | low);
    }
    return true;
}

bool has_enabled_admin(const AccessControl& access)
{
    for (std::size_t i = 0; i < access.user_count(); ++i) {
        const auto* user = access.user_at(i);
        if (user != nullptr && user->enabled && user->role == AccessRole::Admin) return true;
    }
    return false;
}
}

void AccessBootstrapHttp::generate_token()
{
    static constexpr char digits[] = "0123456789ABCDEF";
    for (std::size_t i = 0; i < 16; ++i) token_[i] = digits[esp_random() & 0x0fU];
    token_[16] = '\0';
    token_valid_ = true;
    ESP_LOGW(kTag, "FIRST ADMIN BOOTSTRAP TOKEN: %s", token_.data());
}

esp_err_t AccessBootstrapHttp::register_handlers(httpd_handle_t server, AccessControl* access, AccessNvsStore* store)
{
    if (server == nullptr || access == nullptr || store == nullptr) return ESP_ERR_INVALID_ARG;
    access_ = access;
    store_ = store;
    if (access_->user_count() == 0) generate_token();

    const httpd_uri_t routes[] = {
        {.uri="/api/v1/access/bootstrap/status", .method=HTTP_GET, .handler=&AccessBootstrapHttp::status_get, .user_ctx=this},
        {.uri="/api/v1/access/bootstrap", .method=HTTP_POST, .handler=&AccessBootstrapHttp::bootstrap_post, .user_ctx=this},
        {.uri="/api/v1/access/users/list", .method=HTTP_POST, .handler=&AccessBootstrapHttp::users_list_post, .user_ctx=this},
        {.uri="/api/v1/access/users", .method=HTTP_POST, .handler=&AccessBootstrapHttp::users_upsert_post, .user_ctx=this},
        {.uri="/api/v1/access/config/export", .method=HTTP_POST, .handler=&AccessBootstrapHttp::config_export_post, .user_ctx=this},
        {.uri="/api/v1/access/config/import", .method=HTTP_POST, .handler=&AccessBootstrapHttp::config_import_post, .user_ctx=this},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

AccessBootstrapHttp* AccessBootstrapHttp::self_from(httpd_req_t* request)
{
    return request == nullptr ? nullptr : static_cast<AccessBootstrapHttp*>(request->user_ctx);
}

esp_err_t AccessBootstrapHttp::status_get(httpd_req_t* request)
{
    auto* self = self_from(request);
    if (self == nullptr) return ESP_ERR_INVALID_ARG;
    const bool available = self->access_->user_count() == 0 && self->token_valid_;
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request,
        available ? "{\"bootstrap_available\":true}" : "{\"bootstrap_available\":false}", -1);
}

esp_err_t AccessBootstrapHttp::bootstrap_post(httpd_req_t* request)
{
    auto* self = self_from(request);
    if (self == nullptr || self->access_ == nullptr || self->store_ == nullptr) return ESP_ERR_INVALID_ARG;
    if (self->access_->user_count() != 0 || !self->token_valid_) {
        return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "bootstrap closed");
    }

    std::string body;
    if (!receive_body(request, body)) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid bootstrap request");
    }

    std::string token, id, name, pin;
    if (!extract_json_string(body, "bootstrap_token", token) ||
        !extract_json_string(body, "id", id) || id.empty() || id.size() > 23 ||
        !extract_json_string(body, "name", name) || name.empty() || name.size() > 31 ||
        !extract_json_string(body, "pin", pin) || pin.size() < 4 || pin.size() > 12) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "bootstrap fields invalid");
    }
    if (token != self->token_.data()) {
        return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "bootstrap token rejected");
    }

    const auto salt = make_salt();
    if (!self->access_->set_user(id, name, AccessRole::Admin, pin, salt, true)) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "admin creation failed");
    }
    const auto save_error = self->store_->save(*self->access_);
    if (save_error != ESP_OK) {
        self->access_->clear_users();
        ESP_LOGE(kTag, "access NVS save failed: %s", esp_err_to_name(save_error));
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS save failed");
    }

    self->token_.fill('\0');
    self->token_valid_ = false;
    ESP_LOGI(kTag, "First admin created and bootstrap permanently closed");
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, "{\"accepted\":true,\"role\":\"admin\"}", -1);
}

esp_err_t AccessBootstrapHttp::users_list_post(httpd_req_t* request)
{
    auto* self = self_from(request);
    if (self == nullptr || self->access_ == nullptr) return ESP_ERR_INVALID_ARG;
    std::string body;
    if (!receive_body(request, body) || !authorize_admin(*self->access_, body, "access.users.list")) {
        return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "admin authentication required");
    }

    std::string json = "{\"users\":[";
    for (std::size_t i = 0; i < self->access_->user_count(); ++i) {
        const auto* user = self->access_->user_at(i);
        if (user == nullptr) continue;
        if (i != 0) json += ',';
        json += "{\"id\":\"" + json_escape(user->id.data()) + "\",\"name\":\"";
        json += json_escape(user->name.data());
        json += "\",\"role\":\"";
        json += to_string(user->role);
        json += "\",\"enabled\":";
        json += user->enabled ? "true" : "false";
        json += '}';
    }
    json += "]}";
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, json.c_str(), json.size());
}

esp_err_t AccessBootstrapHttp::users_upsert_post(httpd_req_t* request)
{
    auto* self = self_from(request);
    if (self == nullptr || self->access_ == nullptr || self->store_ == nullptr) return ESP_ERR_INVALID_ARG;
    std::string body;
    if (!receive_body(request, body) || !authorize_admin(*self->access_, body, "access.users.manage")) {
        return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "admin authentication required");
    }

    std::string id, name, role_text, pin;
    AccessRole role{};
    if (!extract_json_string(body, "id", id) || id.empty() || id.size() > 23 ||
        !extract_json_string(body, "name", name) || name.empty() || name.size() > 31 ||
        !extract_json_string(body, "role", role_text) || !parse_role(role_text, role) ||
        !extract_json_string(body, "pin", pin) || pin.size() < 4 || pin.size() > 12) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "user fields invalid");
    }

    const AccessControl backup = *self->access_;
    if (!self->access_->set_user(id, name, role, pin, make_salt(), true)) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "user creation failed");
    }
    if (!has_enabled_admin(*self->access_)) {
        *self->access_ = backup;
        httpd_resp_set_status(request, "409 Conflict");
        return httpd_resp_send(request, "at least one admin required", -1);
    }
    const auto save_error = self->store_->save(*self->access_);
    if (save_error != ESP_OK) {
        *self->access_ = backup;
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS save failed");
    }

    std::string response = "{\"accepted\":true,\"id\":\"" + json_escape(id) + "\",\"role\":\"";
    response += to_string(role);
    response += "\"}";
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, response.c_str(), response.size());
}

esp_err_t AccessBootstrapHttp::config_export_post(httpd_req_t* request)
{
    auto* self = self_from(request);
    if (self == nullptr || self->access_ == nullptr) return ESP_ERR_INVALID_ARG;
    std::string body;
    if (!receive_body(request, body) || !authorize_admin(*self->access_, body, "access.config.export")) {
        return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "admin authentication required");
    }

    const auto image = AccessStoreCodec::encode(*self->access_);
    const std::string response = "{\"format\":\"homeguard-access-v1\",\"image_hex\":\"" + image_to_hex(image) + "\"}";
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, response.c_str(), response.size());
}

esp_err_t AccessBootstrapHttp::config_import_post(httpd_req_t* request)
{
    auto* self = self_from(request);
    if (self == nullptr || self->access_ == nullptr || self->store_ == nullptr) return ESP_ERR_INVALID_ARG;
    std::string body;
    if (!receive_body(request, body) || !authorize_admin(*self->access_, body, "access.config.import")) {
        return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "admin authentication required");
    }

    std::string image_hex;
    AccessStoreCodec::Image image{};
    if (!extract_json_string(body, "image_hex", image_hex) || !image_from_hex(image_hex, image)) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid backup image");
    }

    AccessControl candidate{};
    if (!AccessStoreCodec::decode(image, candidate) || !has_enabled_admin(candidate)) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "backup rejected");
    }
    const auto save_error = self->store_->save(candidate);
    if (save_error != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS save failed");
    }
    *self->access_ = candidate;
    self->token_.fill('\0');
    self->token_valid_ = false;

    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, "{\"accepted\":true,\"restart_required\":false}", -1);
}

}  // namespace homeguard::idf
