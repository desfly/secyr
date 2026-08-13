#pragma once

#include "hg_cloud_nvs.hpp"
#include "hg_network_http.hpp"
#include "homeguard/access_control.hpp"
#include "esp_err.h"
#include "esp_http_server.h"

namespace homeguard::idf {

class ConfigHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server,
                                AccessControl* access,
                                NetworkHttp* network,
                                CloudNvsStore* cloud_store);

private:
    static esp_err_t config_post(httpd_req_t* request);
    esp_err_t handle_config(httpd_req_t* request);
    esp_err_t handle_export(httpd_req_t* request);
    esp_err_t handle_import(httpd_req_t* request, const std::string& body);

    AccessControl* access_{};
    NetworkHttp* network_{};
    CloudNvsStore* cloud_store_{};
};

}  // namespace homeguard::idf
