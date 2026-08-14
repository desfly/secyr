#pragma once

#include "hg_input_nvs.hpp"
#include "hg_input_runtime.hpp"
#include "homeguard/access_control.hpp"
#include "esp_http_server.h"

#include <string>

namespace homeguard::idf {

class InputsHttp {
public:
    esp_err_t register_handlers(
        httpd_handle_t server,
        InputRuntime* runtime,
        InputNvsStore* store,
        homeguard::AccessControl* access_control);

private:
    static esp_err_t inputs_get(httpd_req_t* request);
    static esp_err_t polarity_get(httpd_req_t* request);
    static esp_err_t polarity_put(httpd_req_t* request);
    esp_err_t send_json(httpd_req_t* request, const std::string& body) const;

    InputRuntime* runtime_{};
    InputNvsStore* store_{};
    homeguard::AccessControl* access_control_{};
};

}  // namespace homeguard::idf
