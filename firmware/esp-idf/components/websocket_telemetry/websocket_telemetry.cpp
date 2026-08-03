#include "websocket_telemetry.hpp"
#include "esp_https_server.h"
#include "esp_log.h"
#include <array>
#include <new>
#include <string>
#include <utility>

namespace { constexpr char tag[] = "hg_ws"; }

struct WebsocketTelemetry::BroadcastWork {
    WebsocketTelemetry* owner{};
    std::array<int, 4> clients{{-1, -1, -1, -1}};
    std::string payload;
};

bool WebsocketTelemetry::begin(void* server_handle, std::string_view local_api_token) {
    if (server_ || !server_handle || local_api_token.size() < 32U) return false;
    server_ = server_handle;
    token_.reset(local_api_token);
    httpd_uri_t uri{};
    uri.uri = "/ws/telemetry";
    uri.method = HTTP_GET;
    uri.handler = &WebsocketTelemetry::websocket_entry;
    uri.user_ctx = this;
    uri.is_websocket = true;
    if (httpd_register_uri_handler(static_cast<httpd_handle_t>(server_), &uri) != ESP_OK) {
        stop();
        return false;
    }
    ESP_LOGI(tag, "authenticated WSS telemetry registered");
    return true;
}

void WebsocketTelemetry::stop() {
    std::scoped_lock lock(mutex_);
    clients_.fill(-1);
    token_.clear();
    server_ = nullptr;
}

bool WebsocketTelemetry::authorize(httpd_req_t* request) const {
    const size_t length = httpd_req_get_hdr_value_len(request, "Authorization");
    if (length == 0U || length > 320U) return false;
    std::array<char, 321> value{};
    if (httpd_req_get_hdr_value_str(request, "Authorization", value.data(), length + 1U) != ESP_OK) return false;
    return token_.authorized(std::string_view(value.data(), length));
}

void WebsocketTelemetry::add_client(int fd) {
    std::scoped_lock lock(mutex_);
    for (const int existing : clients_) if (existing == fd) return;
    for (int& slot : clients_) if (slot < 0) { slot = fd; return; }
}

void WebsocketTelemetry::remove_client(int fd) {
    std::scoped_lock lock(mutex_);
    for (int& slot : clients_) if (slot == fd) slot = -1;
}

int WebsocketTelemetry::websocket_entry(httpd_req_t* request) {
    return static_cast<WebsocketTelemetry*>(request->user_ctx)->websocket(request);
}

int WebsocketTelemetry::websocket(httpd_req_t* request) {
    const int fd = httpd_req_to_sockfd(request);
    if (request->method == HTTP_GET) {
        if (!authorize(request)) {
            httpd_resp_set_status(request, "401 Unauthorized");
            httpd_resp_set_hdr(request, "WWW-Authenticate", "Bearer realm=\"homeguard-telemetry\"");
            return httpd_resp_send(request, nullptr, 0);
        }
        add_client(fd);
        return ESP_OK;
    }

    httpd_ws_frame_t frame{};
    frame.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t result = httpd_ws_recv_frame(request, &frame, 0);
    if (result != ESP_OK) { remove_client(fd); return result; }
    if (frame.len > 64U) { remove_client(fd); return ESP_FAIL; }
    std::array<uint8_t, 65> payload{};
    frame.payload = payload.data();
    result = httpd_ws_recv_frame(request, &frame, frame.len);
    if (result != ESP_OK) { remove_client(fd); return result; }
    if (frame.type == HTTPD_WS_TYPE_CLOSE) remove_client(fd);
    if (frame.type == HTTPD_WS_TYPE_PING) {
        frame.type = HTTPD_WS_TYPE_PONG;
        return httpd_ws_send_frame(request, &frame);
    }
    return ESP_OK;
}

void WebsocketTelemetry::broadcast_work_entry(void* context) {
    auto* work = static_cast<BroadcastWork*>(context);
    if (!work) return;
    if (work->owner) work->owner->run_broadcast(*work);
    delete work;
}

void WebsocketTelemetry::run_broadcast(BroadcastWork& work) {
    if (!server_) return;
    httpd_ws_frame_t packet{};
    packet.type = HTTPD_WS_TYPE_TEXT;
    packet.payload = reinterpret_cast<uint8_t*>(work.payload.data());
    packet.len = work.payload.size();
    for (const int fd : work.clients) {
        if (fd < 0) continue;
        if (httpd_ws_send_frame_async(static_cast<httpd_handle_t>(server_), fd, &packet) != ESP_OK) {
            remove_client(fd);
        }
    }
}

void WebsocketTelemetry::publish(const hg::TelemetryFrame& frame) {
    void* handle = nullptr;
    auto* work = new (std::nothrow) BroadcastWork{};
    if (!work) {
        ESP_LOGE(tag, "telemetry broadcast allocation failed");
        return;
    }
    {
        std::scoped_lock lock(mutex_);
        handle = server_;
        if (!handle) { delete work; return; }
        work->owner = this;
        work->clients = clients_;
        work->payload = hg::telemetry_json(frame);
    }
    // HTTPS server APIs are not thread-safe; execute the send inside the HTTP server task.
    if (httpd_queue_work(static_cast<httpd_handle_t>(handle), &WebsocketTelemetry::broadcast_work_entry, work) != ESP_OK) {
        delete work;
        ESP_LOGW(tag, "telemetry broadcast queue is full");
    }
}
