#pragma once

#include "homeguard/access_control.hpp"
#include "esp_err.h"
#include "esp_http_server.h"

namespace homeguard::idf {

class BuildHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server, homeguard::AccessControl* access_control);

private:
    static esp_err_t build_get(httpd_req_t* request);

    homeguard::AccessControl* access_control_{};
};

}  // namespace homeguard::idf
