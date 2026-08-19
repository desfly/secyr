#pragma once

#include "hg_access_nvs.hpp"
#include "hg_access_runtime.hpp"
#include "homeguard/access_control.hpp"
#include "esp_err.h"
#include "esp_http_server.h"

namespace homeguard::idf {

class AccessHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server,
                                AccessControl* access,
                                AccessNvsStore* store,
                                bool bootstrap_allowed);

private:
    class BootstrapFlag {
    public:
        BootstrapFlag& operator=(bool value) noexcept {
            value_ = value;
            access_runtime::set_bootstrap_allowed(value);
            return *this;
        }
        [[nodiscard]] operator bool() const noexcept { return value_; }

    private:
        bool value_{};
    };

    static esp_err_t state_get(httpd_req_t* request);
    static esp_err_t users_post(httpd_req_t* request);
    static esp_err_t login_post(httpd_req_t* request);

    esp_err_t handle_state(httpd_req_t* request);
    esp_err_t handle_users(httpd_req_t* request);
    esp_err_t handle_login(httpd_req_t* request);

    AccessControl* access_{};
    AccessNvsStore* store_{};
    BootstrapFlag bootstrap_allowed_{};
};

}  // namespace homeguard::idf
