#include "hg_cloud_http.hpp"

#include "hg_cloud_link.hpp"

#include <string>

namespace homeguard::idf {

esp_err_t CloudHttp::register_handlers(httpd_handle_t server, CloudLink* cloud)
{
    if (server == nullptr || cloud == nullptr) return ESP_ERR_INVALID_ARG;
    cloud_ = cloud;
    const httpd_uri_t route = {
        .uri = "/api/v1/cloud/status",
        .method = HTTP_GET,
        .handler = &CloudHttp::status_get,
        .user_ctx = this,
    };
    return httpd_register_uri_handler(server, &route);
}

esp_err_t CloudHttp::status_get(httpd_req_t* request)
{
    if (request == nullptr || request->user_ctx == nullptr) return ESP_ERR_INVALID_ARG;
    auto* self = static_cast<CloudHttp*>(request->user_ctx);
    if (self->cloud_ == nullptr) return ESP_FAIL;

    const bool configured = self->cloud_->configured();
    const bool connected = self->cloud_->connected();
    const char* state = connected ? "connected" : (configured ? "connecting" : "disabled");
    const std::string body =
        std::string{"{\"ok\":true,\"state\":\""} + state +
        "\",\"configured\":" + (configured ? "true" : "false") +
        ",\"connected\":" + (connected ? "true" : "false") +
        ",\"deviceId\":\"" + self->cloud_->device_id() +
        "\",\"connectCount\":" + std::to_string(self->cloud_->connect_count()) +
        ",\"disconnectCount\":" + std::to_string(self->cloud_->disconnect_count()) + "}";

    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

}  // namespace homeguard::idf
