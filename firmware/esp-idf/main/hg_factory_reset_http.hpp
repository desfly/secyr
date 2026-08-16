#pragma once

#include "hg_factory_reset.hpp"
#include "homeguard/access_control.hpp"
#include "esp_http_server.h"

namespace homeguard::idf {

class FactoryResetHttp {
public:
    esp_err_t register_handlers(
        httpd_handle_t server,
        homeguard::AccessControl* access_control,
        FactoryResetManager* reset_manager);

private:
    static esp_err_t factory_reset_post(httpd_req_t* request);
    esp_err_t handle_factory_reset(httpd_req_t* request);

    homeguard::AccessControl* access_control_{};
    FactoryResetManager* reset_manager_{};
};

}  // namespace homeguard::idf
