#pragma once

#include "hg_commissioning_nvs.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/boot_readiness.hpp"
#include "homeguard/commissioning_state.hpp"
#include "homeguard/hardware_verification.hpp"
#include "homeguard/physical_output_runtime.hpp"
#include "homeguard/system_model.hpp"
#include "esp_http_server.h"

#include <cstdint>
#include <mutex>
#include <string>

namespace homeguard::idf {

class HardwareBootstrap;

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
        hg::SystemModel* model,
        HardwareBootstrap* hardware_runtime,
        std::mutex* control_state_mutex);

    void set_access_control(homeguard::AccessControl* access_control) {
        access_control_ = access_control;
    }

private:
    static esp_err_t readiness_get(httpd_req_t* request);
    static esp_err_t maintenance_post(httpd_req_t* request);
    static esp_err_t hardware_verify_post(httpd_req_t* request);
    static esp_err_t dry_run_post(httpd_req_t* request);
    static esp_err_t valve_profile_post(httpd_req_t* request);
    static esp_err_t bench_pulse_post(httpd_req_t* request);
    static esp_err_t actuator_accept_post(httpd_req_t* request);
    static esp_err_t invalidate_post(httpd_req_t* request);
    static esp_err_t factory_reset_post(httpd_req_t* request);

    esp_err_t send_json(httpd_req_t* request, const std::string& body) const;
    void refresh_control_state_from_store();
    [[nodiscard]] bool maintenance_active() const;

    CommissioningNvsStore* store_{};
    hg::HardwareVerificationRecord* hardware_{};
    hg::CommissioningPersistentState* commissioning_{};
    hg::BootReadinessReport* readiness_{};
    hg::PhysicalOutputRuntime* physical_outputs_{};
    hg::SystemEventBus* bus_{};
    hg::SystemModel* model_{};
    HardwareBootstrap* hardware_runtime_{};
    std::mutex* control_state_mutex_{};
    homeguard::AccessControl* access_control_{};
    bool maintenance_active_{};
    std::uint8_t bench_valve_mask_{};
};

}  // namespace homeguard::idf
