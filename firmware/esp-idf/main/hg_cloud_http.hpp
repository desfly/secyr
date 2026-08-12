#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

namespace homeguard { class AccessControl; }

namespace homeguard::idf {

class CloudLink;
class CloudNvsStore;

class CloudHttp {
public:
    esp_err_t register_handlers(
        httpd_handle_t server,
        CloudLink* cloud,
        CloudNvsStore* store,
        homeguard::AccessControl* access_control);

private:
    static esp_err_t status_get(httpd_req_t* request);
    static esp_err_t config_post(httpd_req_t* request);
    esp_err_t handle_config(httpd_req_t* request);

    CloudLink* cloud_{};
    CloudNvsStore* store_{};
    homeguard::AccessControl* access_control_{};
};

}  // namespace homeguard::idf
