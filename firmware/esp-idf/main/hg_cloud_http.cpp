#include "hg_cloud_http.hpp"

#include "hg_cloud_link.hpp"

#include <cstdio>
#include <cstring>

namespace homeguard::idf {

esp_err_t CloudHttp::register_handlers(httpd_handle_t server, CloudLink* cloud)
{
    if (server == nullptr || cloud == nullptr) return ESP_ERR_INVALID_ARG;
    cloud_ = cloud;
    const httpd_uri_t route{
        .uri = "/api/v1/cloud/status",
        .method = HTTP_GET,
        .handler = &CloudHttp::status_get,
        .user_ctx = this,
    };
    return httpd_register_uri_handler(server, &route);
}

esp_err_t CloudHttp::status_get(httpd_req_t* request)
{
    if (request == nullptr) return ESP_ERR_INVALID_ARG;
    auto* self = static_cast<CloudHttp*>(request->user_ctx);
    if (self == nullptr || self->cloud_ == nullptr) return ESP_ERR_INVALID_STATE;

    char body[256]{};
    std::snprintf(body, sizeof(body),
                  "{\"ok\":true,\"configured\":%s,\"connected\":%s,\"device_id\":\"%s\",\"connect_count\":%u,\"disconnect_count\":%u}",
                  self->cloud_->configured() ? "true" : "false",
                  self->cloud_->connected() ? "true" : "false",
                  self->cloud_->device_id(),
                  static_cast<unsigned>(self->cloud_->connect_count()),
                  static_cast<unsigned>(self->cloud_->disconnect_count()));
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, body, static_cast<ssize_t>(std::strlen(body)));
}

}  // namespace homeguard::idf
