#pragma once

#include "homeguard/system_model.hpp"
#include "esp_http_server.h"
#include <array>
#include <cstddef>

namespace homeguard::idf {

class SystemHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server, hg::SystemModel* model, hg::SystemEventBus* bus);

private:
    static esp_err_t status_get(httpd_req_t* request);
    static esp_err_t zones_get(httpd_req_t* request);
    static esp_err_t outputs_get(httpd_req_t* request);
    static esp_err_t partitions_get(httpd_req_t* request);
    static esp_err_t websocket(httpd_req_t* request);
    static void on_event(const hg::SystemEvent& event, void* context);

    esp_err_t send_json(httpd_req_t* request, const char* body, std::size_t size) const;
    void remember_client(int socket_fd);
    void broadcast(const hg::SystemEvent& event);

    httpd_handle_t server_{};
    hg::SystemModel* model_{};
    hg::SystemEventBus* bus_{};
    std::array<int, 4> clients_{{-1, -1, -1, -1}};
};

}  // namespace homeguard::idf
