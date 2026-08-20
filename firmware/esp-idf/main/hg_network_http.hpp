#pragma once

#include "homeguard/access_control.hpp"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace homeguard::idf {

class NetworkHttp {
public:
    esp_err_t begin();
    void set_access_control(AccessControl* access) { access_ = access; }
    [[nodiscard]] AccessControl* access_control() const noexcept { return access_; }
    esp_err_t register_handlers(httpd_handle_t server);

private:
    enum class StaState : std::uint8_t {
        Idle,
        Connecting,
        Connected,
        Error,
    };

    enum class AsyncOperation : std::uint8_t {
        Scan,
        Connect,
    };

    struct AsyncContext {
        NetworkHttp* self{};
        httpd_req_t* request{};
        AsyncOperation operation{AsyncOperation::Scan};
        bool setup_mode{};
        std::string ssid;
        std::string password;
    };

    struct CandidateTimeoutContext {
        NetworkHttp* self{};
        std::uint32_t generation{};
    };

    static esp_err_t status_get(httpd_req_t* request);
    static esp_err_t scan_get(httpd_req_t* request);
    static esp_err_t connect_post(httpd_req_t* request);

    static void async_task_entry(void* argument);
    static void reconnect_task_entry(void* argument);
    static void candidate_timeout_task_entry(void* argument);
    static void wifi_event_entry(void* argument, esp_event_base_t base, std::int32_t id, void* data);
    static void ip_event_entry(void* argument, esp_event_base_t base, std::int32_t id, void* data);

    esp_err_t handle_status(httpd_req_t* request);
    esp_err_t handle_scan(httpd_req_t* request);
    esp_err_t handle_connect(httpd_req_t* request);

    esp_err_t dispatch_async(httpd_req_t* request,
                             AsyncOperation operation,
                             bool setup_mode = false,
                             std::string ssid = {},
                             std::string password = {});
    void process_async(AsyncContext& context);
    void process_scan(AsyncContext& context);
    void process_connect(AsyncContext& context);

    void on_wifi_event(std::int32_t id, void* data);
    void on_ip_event(std::int32_t id, void* data);
    void schedule_reconnect_retry();
    bool start_candidate_timeout(std::uint32_t generation);
    void cancel_candidate_and_restore_active(const char* reason);
    bool restore_active_config_only();
    void restore_active_connection();

    bool apply_sta(const std::string& ssid, const std::string& password, bool persist);
    bool load_credentials(std::string& ssid, std::string& password) const;
    bool save_credentials(const std::string& ssid, const std::string& password) const;
    bool clear_credentials() const;
    bool load_candidate_credentials(std::string& ssid, std::string& password) const;
    bool save_candidate_credentials(const std::string& ssid, const std::string& password) const;
    bool clear_candidate_credentials() const;
    bool load_credentials_from_key(const char* key, std::string& ssid, std::string& password) const;
    bool save_credentials_to_key(const char* key, const std::string& ssid, const std::string& password) const;
    bool clear_credentials_key(const char* key) const;

    [[nodiscard]] std::string status_json() const;
    [[nodiscard]] std::string scan_json() const;
    [[nodiscard]] const char* state_name(StaState state) const noexcept;

    AccessControl* access_{};
    void* sta_netif_{};
    std::string ap_ssid_;

    std::atomic<StaState> sta_state_{StaState::Idle};
    std::atomic<int> last_disconnect_reason_{0};
    std::atomic<unsigned> reconnect_attempts_{0};
    std::atomic<unsigned> candidate_attempts_{0};
    std::atomic<bool> async_busy_{false};
    std::atomic<bool> candidate_pending_{false};
    std::atomic<bool> suppress_disconnect_reconnect_{false};
    std::atomic<bool> reconnect_scheduled_{false};
    std::atomic<bool> restore_in_progress_{false};
    std::atomic<std::uint32_t> candidate_generation_{0};
    bool initialized_{};
};

}  // namespace homeguard::idf
