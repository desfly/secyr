#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

namespace homeguard::idf {

class HardwareBootstrap;

class InfrastructureHttp {
public:
    esp_err_t register_handlers(
        httpd_handle_t server,
        HardwareBootstrap* hardware);

private:
    static esp_err_t status_get(
        httpd_req_t* request);
};

}  // namespace homeguard::idf
