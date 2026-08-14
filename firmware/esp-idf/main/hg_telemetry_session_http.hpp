#pragma once

#include "homeguard/access_control.hpp"
#include "websocket_telemetry.hpp"
#include "esp_err.h"
#include "esp_http_server.h"

namespace homeguard::idf {

class TelemetrySessionHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server,
                                AccessControl* access,
                                WebsocketTelemetry* telemetry);

private:
    static esp_err_t login_post(httpd_req_t* request);
    esp_err_t handle_login(httpd_req_t* request);

    AccessControl* access_{};
    WebsocketTelemetry* telemetry_{};
};

}  // namespace homeguard::idf
