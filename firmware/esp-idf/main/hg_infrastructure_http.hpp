#pragma once

#include "homeguard/access_control.hpp"
#include "esp_err.h"
#include "esp_http_server.h"

namespace homeguard::idf {

class HardwareBootstrap;

class InfrastructureHttp {
public:
    esp_err_t register_handlers(
        httpd_handle_t server,
        HardwareBootstrap* hardware,
        homeguard::AccessControl* access_control);

private:
    static esp_err_t status_get(httpd_req_t* request);
    static esp_err_t analog_get(httpd_req_t* request);
    static esp_err_t rgb_test_post(httpd_req_t* request);

    HardwareBootstrap* hardware_{};
    homeguard::AccessControl* access_control_{};
};

}  // namespace homeguard::idf
