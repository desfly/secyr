#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

namespace hg { class BleRemoteRegistry; }
namespace homeguard { class AccessControl; }

namespace homeguard::idf {
class BleRemoteNvsStore;

class BleRemoteHttp {
public:
    esp_err_t register_handlers(httpd_handle_t server,
                                hg::BleRemoteRegistry* registry,
                                BleRemoteNvsStore* store,
                                homeguard::AccessControl* access);

private:
    static esp_err_t list_get(httpd_req_t* request);
    static esp_err_t upsert_post(httpd_req_t* request);
    static esp_err_t remove_delete(httpd_req_t* request);

    esp_err_t handle_list(httpd_req_t* request);
    esp_err_t handle_upsert(httpd_req_t* request);
    esp_err_t handle_remove(httpd_req_t* request);

    hg::BleRemoteRegistry* registry_{};
    BleRemoteNvsStore* store_{};
    homeguard::AccessControl* access_{};
};

}  // namespace homeguard::idf
