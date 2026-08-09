#include "hg_access_bootstrap_http.hpp"

#include "esp_log.h"
#include "esp_random.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "hg_access_bootstrap";

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
}

void AccessBootstrapHttp::generate_token()
{
    static constexpr char digits[] = "0123456789ABCDEF";
    for (std::size_t i = 0; i < 16; ++i) {
        token_[i] = digits[esp_random() & 0x0fU];
    }
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
    if (request->content_len == 0 || request->content_len > 512) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid bootstrap request");
    }

    std::array<char, 513> buffer{};
    const auto received = httpd_req_recv(request, buffer.data(), request->content_len);
    if (received <= 0) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "body read failed");

    const std::string body(buffer.data(), static_cast<std::size_t>(received));
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

}  // namespace homeguard::idf
