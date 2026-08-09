#include "hg_cloud_http.hpp"

#include "esp_log.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <string>

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "hg_cloud_http";

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

esp_err_t CloudHttp::register_handlers(httpd_handle_t server, CloudConfigStore* store, CloudLink* link)
{
    if (server == nullptr || store == nullptr || link == nullptr) return ESP_ERR_INVALID_ARG;
    store_ = store;
    link_ = link;
    const httpd_uri_t routes[] = {
        {.uri="/api/v1/cloud/status", .method=HTTP_GET, .handler=&CloudHttp::status_get, .user_ctx=this},
        {.uri="/api/v1/cloud/config", .method=HTTP_POST, .handler=&CloudHttp::config_post, .user_ctx=this},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

CloudHttp* CloudHttp::self_from(httpd_req_t* request)
{
    return request == nullptr ? nullptr : static_cast<CloudHttp*>(request->user_ctx);
}

esp_err_t CloudHttp::status_get(httpd_req_t* request)
{
    auto* self = self_from(request);
    if (self == nullptr) return ESP_ERR_INVALID_ARG;
    std::string body = "{\"device_id\":\"";
    body += self->link_->device_id();
    body += "\",\"configured\":";
    body += self->link_->configured() ? "true" : "false";
    body += ",\"connected\":";
    body += self->link_->connected() ? "true" : "false";
    body += ",\"connect_count\":" + std::to_string(self->link_->connect_count());
    body += ",\"disconnect_count\":" + std::to_string(self->link_->disconnect_count());
    body += ",\"command_count\":" + std::to_string(self->link_->command_count());
    body += "}";
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

esp_err_t CloudHttp::config_post(httpd_req_t* request)
{
    auto* self = self_from(request);
    if (self == nullptr || request->content_len == 0 || request->content_len > 768) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid cloud request");
    }

    std::array<char, 769> buffer{};
    const auto received = httpd_req_recv(request, buffer.data(), request->content_len);
    if (received <= 0) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "body read failed");

    const std::string body(buffer.data(), static_cast<std::size_t>(received));
    std::string uri, username, password;
    if (!extract_json_string(body, "broker_uri", uri) || uri.empty() || uri.size() >= 160 ||
        !extract_json_string(body, "username", username) || username.size() >= 65 ||
        !extract_json_string(body, "password", password) || password.size() >= 129) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "cloud fields invalid");
    }
    if (uri.rfind("mqtts://", 0) != 0 && uri.rfind("wss://", 0) != 0) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "TLS cloud URI required");
    }

    CloudConfig config{};
    std::memcpy(config.broker_uri.data(), uri.data(), uri.size());
    std::memcpy(config.username.data(), username.data(), username.size());
    std::memcpy(config.password.data(), password.data(), password.size());

    auto error = self->store_->save(config);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "cloud config save failed: %s", esp_err_to_name(error));
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS save failed");
    }

    self->link_->stop();
    error = self->link_->start(config.broker_uri.data(), config.username.data(), config.password.data());
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "cloud link start failed: %s", esp_err_to_name(error));
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "cloud start failed");
    }

    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, "{\"accepted\":true,\"state\":\"connecting\"}", -1);
}

}  // namespace homeguard::idf
