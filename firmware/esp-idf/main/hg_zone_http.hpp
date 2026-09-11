#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

namespace homeguard { class AccessControl; }

namespace homeguard::idf {

class ZoneMonitor;

class ZoneHttp {
public:
    esp_err_t register_handlers(
        httpd_handle_t server,
        ZoneMonitor* monitor,
        homeguard::AccessControl* access_control);

private:
    static esp_err_t live_get(httpd_req_t* request);
    static esp_err_t name_post(httpd_req_t* request);

    ZoneMonitor* monitor_{nullptr};
    homeguard::AccessControl* access_control_{nullptr};
};

}  // namespace homeguard::idf
