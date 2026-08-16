#pragma once

#include "hg_commissioning_nvs.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/boot_readiness.hpp"
#include "homeguard/commissioning_state.hpp"
#include "homeguard/hardware_verification.hpp"
#include "homeguard/physical_output_runtime.hpp"
#include "homeguard/system_model.hpp"
#include "esp_http_server.h"

#include <mutex>
#include <string>

namespace homeguard::idf {

class ServiceHttp {
public:
    esp_err_t register_handlers(
        httpd_handle_t server,
        CommissioningNvsStore* store,
        hg::HardwareVerificationRecord* hardware,
        hg::CommissioningPersistentState* commissioning,
        hg::BootReadinessReport* readiness,
        hg::PhysicalOutputRuntime* physical_outputs,
        hg::SystemEventBus* bus,
        std::mutex* control_state_mutex);

    void set_access_control(homeguard::AccessControl* access_control) {
        access_control_ = access_control;
    }

private:
    static esp_err_t readiness_get(httpd_req_t* request);
    static esp_err_t invalidate_post(httpd_req_t* request);
    static esp_err_t factory_reset_post(httpd_req_t* request);
    esp_err_t send_json(httpd_req_t* request, const std::string& body) const;

    CommissioningNvsStore* store_{};
    hg::HardwareVerificationRecord* hardware_{};
    hg::CommissioningPersistentState* commissioning_{};
    hg::BootReadinessReport* readiness_{};
    hg::PhysicalOutputRuntime* physical_outputs_{};
    hg::SystemEventBus* bus_{};
    std::mutex* control_state_mutex_{};
    homeguard::AccessControl* access_control_{};
};

}  // namespace homeguard::idf
