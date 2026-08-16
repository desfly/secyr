#pragma once

#include "homeguard/access_control.hpp"
#include "esp_err.h"
#include "esp_http_server.h"

#include <string>

namespace homeguard::idf {

class NetworkHttp {
public:
    esp_err_t begin();
    void set_access_control(AccessControl* access) { access_ = access; }
    esp_err_t register_handlers(httpd_handle_t server);

    // Backup/restore accessors. These touch persisted credentials only and do
    // not change the live STA connection; imported settings become active after
    // the controlled reboot performed by the configuration API.
    bool load_persisted_credentials(std::string& ssid, std::string& password) const {
        return load_credentials(ssid, password);
    }
    bool save_persisted_credentials(const std::string& ssid, const std::string& password) const {
        if (ssid.empty() || ssid.size() > 32 || password.size() > 64 ||
            (!password.empty() && password.size() < 8)) {
            return false;
        }
        return save_credentials(ssid, password);
    }

private:
    static esp_err_t status_get(httpd_req_t* request);
    static esp_err_t scan_get(httpd_req_t* request);
    static esp_err_t connect_post(httpd_req_t* request);

    esp_err_t handle_status(httpd_req_t* request);
    esp_err_t handle_scan(httpd_req_t* request);
    esp_err_t handle_connect(httpd_req_t* request);

    bool apply_sta(const std::string& ssid, const std::string& password, bool persist);
    bool load_credentials(std::string& ssid, std::string& password) const;
    bool save_credentials(const std::string& ssid, const std::string& password) const;
    std::string status_json() const;
    std::string scan_json() const;

    AccessControl* access_{};
    void* sta_netif_{};
    std::string ap_ssid_;
    bool initialized_{};
};

}  // namespace homeguard::idf
