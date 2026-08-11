#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#include <string>

namespace homeguard::idf {

class NetworkHttp {
public:
    esp_err_t begin();
    esp_err_t register_handlers(httpd_handle_t server);

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

    void* sta_netif_{};
    std::string ap_ssid_;
    bool initialized_{};
};

}  // namespace homeguard::idf
