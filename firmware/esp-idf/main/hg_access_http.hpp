#pragma once

#include "hg_access_nvs.hpp"
#include "homeguard/access_control.hpp"
#include "esp_err.h"
#include "esp_http_server.h"

namespace homeguard::idf {

class AccessHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server,
                                AccessControl* access,
                                AccessNvsStore* store,
                                bool* bootstrap_allowed);

private:
    static esp_err_t state_get(httpd_req_t* request);
    static esp_err_t users_post(httpd_req_t* request);
    static esp_err_t login_post(httpd_req_t* request);

    esp_err_t handle_state(httpd_req_t* request);
    esp_err_t handle_users(httpd_req_t* request);
    esp_err_t handle_login(httpd_req_t* request);
    [[nodiscard]] bool setup_required() const;

    AccessControl* access_{};
    AccessNvsStore* store_{};
    bool* bootstrap_allowed_{};
};

}  // namespace homeguard::idf
