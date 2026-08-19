#include "websocket_telemetry.hpp"
#include "esp_https_server.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include <array>
#include <new>
#include <string>
#include <utility>

namespace {
constexpr char tag[] = "hg_ws";
constexpr std::int64_t kSessionTokenLifetimeUs = 60'000'000;
}

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
    ESP_LOGI(tag, "authenticated telemetry websocket registered");
    return true;
}

std::string WebsocketTelemetry::issue_session_token() {
    std::array<std::uint8_t, 32> random{};
    esp_fill_random(random.data(), random.size());
    static constexpr char hex[] = "0123456789abcdef";
    std::string raw;
    raw.resize(random.size() * 2U);
    for (std::size_t i = 0; i < random.size(); ++i) {
        raw[i * 2U] = hex[(random[i] >> 4U) & 0x0fU];
        raw[i * 2U + 1U] = hex[random[i] & 0x0fU];
    }
    {
        std::scoped_lock lock(mutex_);
        session_tokens_[next_session_token_].reset(raw);
        session_token_issued_us_[next_session_token_] = esp_timer_get_time();
        next_session_token_ = (next_session_token_ + 1U) % session_tokens_.size();
    }
    return raw;
}

void WebsocketTelemetry::stop() {
    std::scoped_lock lock(mutex_);
    clients_.fill(-1);
    token_.clear();
    for (auto& session : session_tokens_) session.clear();
    session_token_issued_us_.fill(0);
    next_session_token_ = 0U;
    server_ = nullptr;
}

bool WebsocketTelemetry::authorize(httpd_req_t* request) {
    const size_t length = httpd_req_get_hdr_value_len(request, "Authorization");
    if (length == 0U || length > 320U) return false;
    std::array<char, 321> value{};
    if (httpd_req_get_hdr_value_str(request, "Authorization", value.data(), length + 1U) != ESP_OK) return false;
    const std::string_view authorization(value.data(), length);
    std::scoped_lock lock(mutex_);
    if (token_.authorized(authorization)) return true;

    const auto now_us = esp_timer_get_time();
    for (std::size_t i = 0; i < session_tokens_.size(); ++i) {
        auto& session = session_tokens_[i];
        if (!session.configured()) continue;
        const auto issued_us = session_token_issued_us_[i];
        if (issued_us <= 0 || now_us - issued_us > kSessionTokenLifetimeUs) {
            session.clear();
            session_token_issued_us_[i] = 0;
            continue;
        }
        if (session.authorized(authorization)) {
            // Session tokens are handshake tickets, not reusable bearer tokens.
            // Consume after the first successful WebSocket upgrade.
            session.clear();
            session_token_issued_us_[i] = 0;
            return true;
        }
    }
    return false;
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
    if (httpd_queue_work(static_cast<httpd_handle_t>(handle), &WebsocketTelemetry::broadcast_work_entry, work) != ESP_OK) {
        delete work;
        ESP_LOGW(tag, "telemetry broadcast queue is full");
    }
}
