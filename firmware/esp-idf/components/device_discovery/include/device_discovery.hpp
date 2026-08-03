#pragma once
#include "homeguard/types.hpp"
#include <atomic>
#include <cstdint>
#include <string>

class DeviceDiscoveryService {
public:
    DeviceDiscoveryService() = default;
    ~DeviceDiscoveryService();
    DeviceDiscoveryService(const DeviceDiscoveryService&) = delete;
    DeviceDiscoveryService& operator=(const DeviceDiscoveryService&) = delete;

    bool begin(std::string device_id, std::string hostname, uint16_t api_port, bool secure = true);
    void stop();
    void set_transport(hg::Transport transport) { transport_.store(transport); }
    [[nodiscard]] bool running() const { return running_.load(); }
private:
    static void udp_task_entry(void* context);
    void udp_task();
    std::string device_id_;
    std::string hostname_;
    std::string instance_;
    std::string hostname_local_;
    std::string port_txt_;
    uint16_t api_port_{443};
    bool secure_{true};
    std::atomic<bool> running_{false};
    std::atomic<hg::Transport> transport_{hg::Transport::None};
    int socket_{-1};
};
