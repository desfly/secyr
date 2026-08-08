#pragma once

#include "hg_commissioning_nvs.hpp"
#include "homeguard/boot_readiness.hpp"
#include "esp_http_server.h"

namespace homeguard::idf {

class CommissioningHttp {
public:
    esp_err_t register_handlers(
        httpd_handle_t server,
        CommissioningNvsStore* store,
        hg::HardwareVerificationRecord* hardware,
        hg::CommissioningPersistentState* commissioning,
        hg::BootReadinessReport* readiness);

private:
    static esp_err_t readiness_get(httpd_req_t* request);
    static esp_err_t invalidate_post(httpd_req_t* request);
    static CommissioningHttp* self_from(httpd_req_t* request);
    esp_err_t send_json(httpd_req_t* request, const std::string& body) const;
    void recompute() noexcept;

    CommissioningNvsStore* store_{};
    hg::HardwareVerificationRecord* hardware_{};
    hg::CommissioningPersistentState* commissioning_{};
    hg::BootReadinessReport* readiness_{};
};

}  // namespace homeguard::idf
