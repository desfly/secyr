#pragma once

#include "hg_access_nvs.hpp"
#include "hg_cloud_nvs.hpp"
#include "hg_commissioning_nvs.hpp"
#include "hg_network_http.hpp"
#include "homeguard/access_control.hpp"
#include "esp_http_server.h"

namespace homeguard::idf {

class ConfigHttp {
public:
    esp_err_t register_handlers(
        httpd_handle_t server,
        homeguard::AccessControl* access,
        AccessNvsStore* access_store,
        NetworkHttp* network,
        CloudNvsStore* cloud_store,
        CommissioningNvsStore* commissioning_store);

private:
    static esp_err_t export_post(httpd_req_t* request);
    static esp_err_t import_post(httpd_req_t* request);

    esp_err_t handle_export(httpd_req_t* request);
    esp_err_t handle_import(httpd_req_t* request);

    homeguard::AccessControl* access_{};
    AccessNvsStore* access_store_{};
    NetworkHttp* network_{};
    CloudNvsStore* cloud_store_{};
    CommissioningNvsStore* commissioning_store_{};
};

}  // namespace homeguard::idf
