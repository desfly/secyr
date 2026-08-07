#include "hg_system_http.hpp"
#include "homeguard/system_api.hpp"

#include <string>

namespace homeguard::idf {

namespace {
SystemHttp* self_from(httpd_req_t* request) {
    return static_cast<SystemHttp*>(request->user_ctx);
}
}

esp_err_t SystemHttp::register_handlers(httpd_handle_t server, hg::SystemModel* model, hg::SystemEventBus* bus) {
    if (server == nullptr || model == nullptr || bus == nullptr) return ESP_ERR_INVALID_ARG;
    server_ = server;
    model_ = model;
    bus_ = bus;
    if (!bus_->subscribe(&SystemHttp::on_event, this)) return ESP_ERR_NO_MEM;

    const httpd_uri_t routes[] = {
        {.uri="/api/v1/system/status", .method=HTTP_GET, .handler=&SystemHttp::status_get, .user_ctx=this},
        {.uri="/api/v1/system/zones", .method=HTTP_GET, .handler=&SystemHttp::zones_get, .user_ctx=this},
        {.uri="/api/v1/system/outputs", .method=HTTP_GET, .handler=&SystemHttp::outputs_get, .user_ctx=this},
        {.uri="/api/v1/system/partitions", .method=HTTP_GET, .handler=&SystemHttp::partitions_get, .user_ctx=this},
        {.uri="/ws/system", .method=HTTP_GET, .handler=&SystemHttp::websocket, .user_ctx=this, .is_websocket=true},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server_, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

esp_err_t SystemHttp::send_json(httpd_req_t* request, const char* body, std::size_t size) const {
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, body, static_cast<ssize_t>(size));
}

esp_err_t SystemHttp::status_get(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->model_ == nullptr || self->bus_ == nullptr) return ESP_FAIL;
    const auto body = hg::system_status_json(*self->model_, *self->bus_);
    return self->send_json(request, body.c_str(), body.size());
}

esp_err_t SystemHttp::zones_get(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->model_ == nullptr) return ESP_FAIL;
    const auto body = hg::system_zones_json(*self->model_);
    return self->send_json(request, body.c_str(), body.size());
}

esp_err_t SystemHttp::outputs_get(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->model_ == nullptr) return ESP_FAIL;
    const auto body = hg::system_outputs_json(*self->model_);
    return self->send_json(request, body.c_str(), body.size());
}

esp_err_t SystemHttp::partitions_get(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->model_ == nullptr) return ESP_FAIL;
    const auto body = hg::system_partitions_json(*self->model_);
    return self->send_json(request, body.c_str(), body.size());
}

void SystemHttp::remember_client(int socket_fd) {
    if (socket_fd < 0) return;
    for (const int client : clients_) if (client == socket_fd) return;
    for (auto& client : clients_) {
        if (client < 0) { client = socket_fd; return; }
    }
    clients_[0] = socket_fd;
}

esp_err_t SystemHttp::websocket(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr) return ESP_FAIL;
    self->remember_client(httpd_req_to_sockfd(request));
    return ESP_OK;
}

void SystemHttp::on_event(const hg::SystemEvent& event, void* context) {
    auto* self = static_cast<SystemHttp*>(context);
    if (self != nullptr) self->broadcast(event);
}

void SystemHttp::broadcast(const hg::SystemEvent& event) {
    if (server_ == nullptr) return;
    const std::string payload = hg::system_event_json(event);
    httpd_ws_frame_t frame{};
    frame.type = HTTPD_WS_TYPE_TEXT;
    frame.payload = reinterpret_cast<std::uint8_t*>(const_cast<char*>(payload.data()));
    frame.len = payload.size();
    for (auto& client : clients_) {
        if (client < 0) continue;
        if (httpd_ws_send_frame_async(server_, client, &frame) != ESP_OK) client = -1;
    }
}

}  // namespace homeguard::idf
