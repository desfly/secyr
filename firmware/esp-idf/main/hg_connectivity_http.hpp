#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

namespace homeguard { class AccessControl; }

namespace homeguard::idf {

class BleTransport;
class CloudLink;
class HardwareBootstrap;
class NetworkHttp;

class ConnectivityHttp {
public:
    esp_err_t register_handlers(
        httpd_handle_t server,
        HardwareBootstrap* hardware,
        NetworkHttp* network,
        BleTransport* ble,
        CloudLink* cloud,
        homeguard::AccessControl* access_control);

private:
    static esp_err_t status_get(httpd_req_t* request);

    HardwareBootstrap* hardware_{};
    NetworkHttp* network_{};
    BleTransport* ble_{};
    CloudLink* cloud_{};
    homeguard::AccessControl* access_control_{};
};

}  // namespace homeguard::idf
