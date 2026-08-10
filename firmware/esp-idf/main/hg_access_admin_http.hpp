#pragma once

#include "esp_http_server.h"
#include "hg_access_nvs.hpp"
#include "homeguard/access_control.hpp"

namespace homeguard::idf {

class AccessAdminHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server, AccessControl* access, AccessNvsStore* store);

private:
    static esp_err_t enable_post(httpd_req_t* request);
    static esp_err_t disable_post(httpd_req_t* request);
    static esp_err_t delete_post(httpd_req_t* request);
    static AccessAdminHttp* self_from(httpd_req_t* request);
    esp_err_t set_enabled(httpd_req_t* request, bool enabled);
    esp_err_t remove(httpd_req_t* request);

    AccessControl* access_{};
    AccessNvsStore* store_{};
};

}  // namespace homeguard::idf
