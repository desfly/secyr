#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

namespace homeguard::idf {

class BuildHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server);

private:
    static esp_err_t build_get(httpd_req_t* request);
};

}  // namespace homeguard::idf
