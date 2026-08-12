#pragma once

#include "hg_cloud_nvs.hpp"
#include "homeguard/access_control.hpp"
#include "esp_err.h"
#include "esp_http_server.h"

namespace homeguard::idf {

class CloudLink;

class CloudHttp {
public:
    esp_err_t initialize(CloudLink* cloud, homeguard::AccessControl* access);
    esp_err_t register_handlers(httpd_handle_t server);

private:
    static esp_err_t status_get(httpd_req_t* request);
    static esp_err_t config_post(httpd_req_t* request);
    esp_err_t handle_config(httpd_req_t* request);
    esp_err_t apply(const CloudConfigRecord& config, bool persist);

    CloudLink* cloud_{};
    homeguard::AccessControl* access_{};
    CloudNvsStore store_{};
    CloudConfigRecord config_{};
};

}  // namespace homeguard::idf
