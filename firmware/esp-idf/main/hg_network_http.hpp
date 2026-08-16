#pragma once

#include "homeguard/access_control.hpp"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_timer.h"

#include <cstdint>
#include <string>

namespace homeguard::idf {

class NetworkHttp {
public:
    esp_err_t begin();
    void set_access_control(AccessControl* access) { access_ = access; }
    esp_err_t register_handlers(httpd_handle_t server);

private:
    static esp_err_t status_get(httpd_req_t* request);
    static esp_err_t scan_get(httpd_req_t* request);
    static esp_err_t connect_post(httpd_req_t* request);
    static void wifi_event_handler(
        void* context,
        esp_event_base_t base,
        std::int32_t id,
        void* data);
    static void ip_event_handler(
        void* context,
        esp_event_base_t base,
        std::int32_t id,
        void* data);
    static void reconnect_timer_handler(void* context);

    esp_err_t handle_status(httpd_req_t* request);
    esp_err_t handle_scan(httpd_req_t* request);
    esp_err_t handle_connect(httpd_req_t* request);
    void handle_wifi_event(std::int32_t id, void* data);
    void handle_ip_event(std::int32_t id, void* data);
    void handle_reconnect_timer();
    void schedule_reconnect();
    void clear_pending_credentials();

    bool apply_sta(const std::string& ssid, const std::string& password);
    bool load_credentials(std::string& ssid, std::string& password) const;
    bool save_credentials(const std::string& ssid, const std::string& password) const;
    std::string status_json() const;
    std::string scan_json() const;

    AccessControl* access_{};
    void* sta_netif_{};
    esp_timer_handle_t reconnect_timer_{};
    std::string ap_ssid_;
    std::string pending_ssid_;
    std::string pending_password_;
    bool initialized_{};
    bool sta_credentials_configured_{};
    bool pending_credentials_{};
    std::uint32_t reconnect_count_{};
};

}  // namespace homeguard::idf
