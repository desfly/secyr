#include "hg_wifi_http.hpp"

#include "esp_log.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <string>

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "hg_wifi_http";

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
}

esp_err_t WifiProvisioningHttp::register_handlers(httpd_handle_t server,
                                                  WifiCredentialStore* store,
                                                  WifiProvisioningRuntime* runtime)
{
    if (server == nullptr || store == nullptr || runtime == nullptr) return ESP_ERR_INVALID_ARG;
    store_ = store;
    runtime_ = runtime;

    const httpd_uri_t provision{
        .uri = "/api/v1/provisioning/wifi",
        .method = HTTP_POST,
        .handler = &WifiProvisioningHttp::provision_post,
        .user_ctx = this,
    };
    const httpd_uri_t status{
        .uri = "/api/v1/wifi/status",
        .method = HTTP_GET,
        .handler = &WifiProvisioningHttp::status_get,
        .user_ctx = this,
    };
    auto error = httpd_register_uri_handler(server, &provision);
    if (error != ESP_OK) return error;
    return httpd_register_uri_handler(server, &status);
}

WifiProvisioningHttp* WifiProvisioningHttp::self_from(httpd_req_t* request)
{
    return request == nullptr ? nullptr : static_cast<WifiProvisioningHttp*>(request->user_ctx);
}

esp_err_t WifiProvisioningHttp::provision_post(httpd_req_t* request)
{
    auto* self = self_from(request);
    if (self == nullptr || request->content_len == 0 || request->content_len > 512) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid provisioning request");
    }

    std::array<char, 513> buffer{};
    const auto received = httpd_req_recv(request, buffer.data(), request->content_len);
    if (received <= 0) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "request body read failed");
    }
    std::string body(buffer.data(), static_cast<std::size_t>(received));
    std::string ssid;
    std::string password;
    if (!extract_json_string(body, "ssid", ssid) || ssid.empty() || ssid.size() > 32 ||
        !extract_json_string(body, "password", password) || password.size() > 64) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "ssid/password invalid");
    }

    WifiCredentials credentials{};
    std::memcpy(credentials.ssid.data(), ssid.data(), ssid.size());
    std::memcpy(credentials.password.data(), password.data(), password.size());
    auto error = self->store_->save(credentials);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "WiFi credentials NVS save failed: %s", esp_err_to_name(error));
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS save failed");
    }

    error = self->runtime_->connect_station(credentials.ssid.data(), credentials.password.data());
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "STA connect start failed: %s", esp_err_to_name(error));
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "STA start failed");
    }

    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, "{\"accepted\":true,\"state\":\"connecting\"}", -1);
}

esp_err_t WifiProvisioningHttp::status_get(httpd_req_t* request)
{
    auto* self = self_from(request);
    if (self == nullptr) return ESP_ERR_INVALID_ARG;
    std::string body = "{\"softap\":";
    body += self->runtime_->started() ? "true" : "false";
    body += ",\"ssid\":\"";
    body += self->runtime_->ssid();
    body += "\",\"ip\":\"";
    body += self->runtime_->ip_address();
    body += "\",\"station\":\"";
    body += self->runtime_->station_connecting() ? "connecting" : "idle";
    body += "\"}";
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

}  // namespace homeguard::idf
