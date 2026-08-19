#pragma once
#include "homeguard/local_api.hpp"
#include "homeguard/telemetry.hpp"
#include "esp_http_server.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

class WebsocketTelemetry {
public:
    bool begin(void* server_handle, std::string_view local_api_token);
    void stop();
    void publish(const hg::TelemetryFrame& frame);
    [[nodiscard]] std::string issue_session_token();
    [[nodiscard]] bool running() const { return server_ != nullptr; }
private:
    struct BroadcastWork;
    static int websocket_entry(httpd_req_t* request);
    static void broadcast_work_entry(void* context);
    int websocket(httpd_req_t* request);
    bool authorize(httpd_req_t* request);
    void add_client(int fd);
    void remove_client(int fd);
    void run_broadcast(BroadcastWork& work);
    void* server_{};
    hg::BearerTokenVerifier token_{};
    std::array<hg::BearerTokenVerifier, 4> session_tokens_{};
    std::array<std::int64_t, 4> session_token_issued_us_{};
    std::size_t next_session_token_{};
    std::array<int, 4> clients_{{-1, -1, -1, -1}};
    mutable std::mutex mutex_{};
};
