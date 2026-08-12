#pragma once

#include "hg_config_nvs.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/config_exchange.hpp"
#include "homeguard/system_model.hpp"
#include "esp_err.h"
#include "esp_http_server.h"

namespace homeguard::idf {

class ConfigHttp {
public:
    esp_err_t initialize(hg::SystemModel* model, AccessControl* access);
    esp_err_t register_handlers(httpd_handle_t server);
    [[nodiscard]] const HomeGuardConfigDocument& document() const { return document_; }

private:
    static esp_err_t export_get(httpd_req_t* request);
    static esp_err_t import_post(httpd_req_t* request);
    esp_err_t handle_export(httpd_req_t* request);
    esp_err_t handle_import(httpd_req_t* request);
    void seed_from_runtime();
    bool can_apply(const HomeGuardConfigDocument& candidate) const;
    bool apply(const HomeGuardConfigDocument& candidate);
    bool authorize_admin(httpd_req_t* request, const char* command);

    hg::SystemModel* model_{};
    AccessControl* access_{};
    ConfigNvsStore store_{};
    HomeGuardConfigDocument document_{};
};

}  // namespace homeguard::idf
