#pragma once

#include "homeguard/access_control.hpp"
#include "homeguard/boot_readiness.hpp"
#include "homeguard/physical_output_runtime.hpp"
#include "homeguard/system_model.hpp"
#include "esp_http_server.h"

#include <mutex>

namespace homeguard::idf {

class OutputHttp {
public:
    esp_err_t register_handlers(
        httpd_handle_t server,
        hg::SystemModel* model,
        hg::BootReadinessReport* readiness,
        hg::PhysicalOutputRuntime* physical,
        hg::SystemEventBus* bus,
        std::mutex* control_state_mutex);

    void set_access_control(homeguard::AccessControl* access_control) {
        access_control_ = access_control;
    }

private:
    static esp_err_t command_post(httpd_req_t* request);
    esp_err_t handle_command(httpd_req_t* request);

    hg::SystemModel* model_{};
    hg::BootReadinessReport* readiness_{};
    hg::PhysicalOutputRuntime* physical_{};
    hg::SystemEventBus* bus_{};
    std::mutex* control_state_mutex_{};
    homeguard::AccessControl* access_control_{};
};

}  // namespace homeguard::idf
