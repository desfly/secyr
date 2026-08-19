#pragma once

#include "homeguard/access_control.hpp"
#include "esp_err.h"
#include "esp_http_server.h"

#include <string>

namespace homeguard::idf {

class LanHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server, homeguard::AccessControl* access_control);

private:
    static esp_err_t devices_get(httpd_req_t* request);
    static esp_err_t scan_get(httpd_req_t* request);

    [[nodiscard]] bool authenticated_request(httpd_req_t* request) const;
    std::string devices_json(bool active_scan) const;

    homeguard::AccessControl* access_control_{};
};

}  // namespace homeguard::idf
