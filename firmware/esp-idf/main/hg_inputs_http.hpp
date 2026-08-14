#pragma once

#include "esp_http_server.h"

namespace homeguard::idf {

class InputsHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server);

private:
    static esp_err_t inputs_get(httpd_req_t* request);
};

}  // namespace homeguard::idf
