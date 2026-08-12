#pragma once

#include "hg_access_nvs.hpp"
#include "homeguard/access_control.hpp"
#include "esp_err.h"
#include "esp_http_server.h"

namespace homeguard::idf {

class AccessHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server, AccessControl* access, AccessNvsStore* store);

private:
    static esp_err_t users_post(httpd_req_t* request);
    esp_err_t handle_users(httpd_req_t* request);

    AccessControl* access_{};
    AccessNvsStore* store_{};
};

}  // namespace homeguard::idf
