#pragma once

#include "homeguard/controller.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

class NetworkManager {
public:
    NetworkManager() = default;
    ~NetworkManager();
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    bool begin(std::string ssid, std::string password);
    bool retry();
    void stop();
    [[nodiscard]] bool connected() const { return connected_.load(); }
    [[nodiscard]] hg::LinkInputs links() const;
    [[nodiscard]] std::string ipv4() const;
    [[nodiscard]] uint32_t disconnect_count() const { return disconnect_count_.load(); }
private:
    static void event_entry(void* context, const char* base, int32_t event_id, void* event_data);
    void handle_event(const char* base, int32_t event_id, void* event_data);
    void rollback_init();
    void clear_password();
    std::string ssid_;
    std::string password_;
    mutable std::mutex state_mutex_;
    std::string ipv4_;
    bool initialized_{};
    bool wifi_initialized_{};
    std::atomic_bool connected_{};
    std::atomic_uint32_t disconnect_count_{};
    void* sta_netif_{};
    void* wifi_event_instance_{};
    void* ip_event_instance_{};
};
