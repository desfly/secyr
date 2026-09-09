#pragma once

#include "esp_http_server.h"

namespace homeguard { class AccessControl; }

namespace homeguard::idf {

class RelayRuntime;

class RelayHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server, RelayRuntime* relays, homeguard::AccessControl* access_control);

private:
    static esp_err_t lock_post(httpd_req_t* request);

    RelayRuntime* relays_{nullptr};
    homeguard::AccessControl* access_control_{nullptr};
};

}  // namespace homeguard::idf
