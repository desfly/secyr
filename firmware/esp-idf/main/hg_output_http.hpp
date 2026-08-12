#pragma once

#include "homeguard/access_control.hpp"
#include "homeguard/boot_readiness.hpp"
#include "homeguard/physical_output_runtime.hpp"
#include "homeguard/system_model.hpp"
#include "homeguard/user_output_access.hpp"
#include "esp_http_server.h"

namespace homeguard::idf {

class OutputHttp {
public:
    esp_err_t register_handlers(
        httpd_handle_t server,
        hg::SystemModel* model,
        hg::BootReadinessReport* readiness,
        hg::PhysicalOutputRuntime* physical,
        hg::SystemEventBus* bus);

    void set_access_control(homeguard::AccessControl* access_control) { access_control_ = access_control; }
    void set_output_access(const homeguard::UserOutputAccess* output_access) { output_access_ = output_access; }

private:
    static esp_err_t command_post(httpd_req_t* request);
    esp_err_t handle_command(httpd_req_t* request);

    hg::SystemModel* model_{};
    hg::BootReadinessReport* readiness_{};
    hg::PhysicalOutputRuntime* physical_{};
    hg::SystemEventBus* bus_{};
    homeguard::AccessControl* access_control_{};
    const homeguard::UserOutputAccess* output_access_{};
};

}  // namespace homeguard::idf
