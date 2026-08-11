#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

namespace homeguard::idf {

class WebHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server);

private:
    static esp_err_t index_get(httpd_req_t* request);
    static esp_err_t css_get(httpd_req_t* request);
    static esp_err_t js_get(httpd_req_t* request);
    static esp_err_t send_asset(httpd_req_t* request,
                                const char* content_type,
                                const unsigned char* start,
                                const unsigned char* end);
};

}  // namespace homeguard::idf
