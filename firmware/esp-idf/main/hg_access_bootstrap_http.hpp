#pragma once

#include "esp_http_server.h"
#include "hg_access_nvs.hpp"
#include "homeguard/access_control.hpp"

#include <array>
#include <cstdint>

namespace homeguard::idf {

class AccessBootstrapHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server, AccessControl* access, AccessNvsStore* store);

private:
    static esp_err_t status_get(httpd_req_t* request);
    static esp_err_t bootstrap_post(httpd_req_t* request);
    static AccessBootstrapHttp* self_from(httpd_req_t* request);
    void generate_token();

    AccessControl* access_{};
    AccessNvsStore* store_{};
    std::array<char, 17> token_{};
    bool token_valid_{};
};

}  // namespace homeguard::idf
