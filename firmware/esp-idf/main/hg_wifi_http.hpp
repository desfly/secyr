#pragma once

#include "hg_wifi_credentials.hpp"
#include "hg_wifi_provisioning.hpp"
#include "esp_http_server.h"

namespace homeguard::idf {

class WifiProvisioningHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server,
                                WifiCredentialStore* store,
                                WifiProvisioningRuntime* runtime);

private:
    static esp_err_t root_get(httpd_req_t* request);
    static esp_err_t provision_post(httpd_req_t* request);
    static esp_err_t status_get(httpd_req_t* request);
    static WifiProvisioningHttp* self_from(httpd_req_t* request);

    WifiCredentialStore* store_{};
    WifiProvisioningRuntime* runtime_{};
};

}  // namespace homeguard::idf
