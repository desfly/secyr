#pragma once

#include "homeguard/boot_readiness.hpp"
#include "homeguard/system_model.hpp"
#include "esp_http_server.h"

namespace homeguard::idf {

class OutputHttp {
public:
    esp_err_t register_handlers(
        httpd_handle_t server,
        hg::SystemModel* model,
        hg::BootReadinessReport* readiness,
        hg::SystemEventBus* bus);

private:
    static esp_err_t command_post(httpd_req_t* request);
    esp_err_t handle_command(httpd_req_t* request);

    hg::SystemModel* model_{};
    hg::BootReadinessReport* readiness_{};
    hg::SystemEventBus* bus_{};
};

}  // namespace homeguard::idf
