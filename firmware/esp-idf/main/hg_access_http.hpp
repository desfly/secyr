#pragma once

#include "hg_access_nvs.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/system_model.hpp"
#include "homeguard/user_output_access.hpp"
#include "homeguard/user_zone_access.hpp"
#include "esp_err.h"
#include "esp_http_server.h"

namespace homeguard::idf {

class AccessHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server,
                                AccessControl* access,
                                AccessNvsStore* store,
                                bool bootstrap_allowed,
                                hg::SystemModel* model,
                                UserZoneAccess* zone_access,
                                UserOutputAccess* output_access);

private:
    static esp_err_t users_post(httpd_req_t* request);
    static esp_err_t login_post(httpd_req_t* request);
    esp_err_t handle_users(httpd_req_t* request);
    esp_err_t handle_login(httpd_req_t* request);

    AccessControl* access_{};
    AccessNvsStore* store_{};
    hg::SystemModel* model_{};
    UserZoneAccess* zone_access_{};
    UserOutputAccess* output_access_{};
    bool bootstrap_allowed_{};
};

}  // namespace homeguard::idf
