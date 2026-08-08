#pragma once

#include "esp_http_server.h"
#include "hg_cloud_config.hpp"
#include "hg_cloud_link.hpp"

namespace homeguard::idf {

class CloudHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server, CloudConfigStore* store, CloudLink* link);

private:
    static esp_err_t status_get(httpd_req_t* request);
    static esp_err_t config_post(httpd_req_t* request);
    static CloudHttp* self_from(httpd_req_t* request);

    CloudConfigStore* store_{};
    CloudLink* link_{};
};

}  // namespace homeguard::idf
