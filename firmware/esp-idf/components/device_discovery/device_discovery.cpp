#include "device_discovery.hpp"
#include "homeguard/discovery.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "mdns.h"
#include <array>
#include <cerrno>
#include <cstring>
#include <string_view>
#include <utility>

namespace {
constexpr char tag[] = "hg_discovery";
constexpr int receive_timeout_ms = 1000;
}

DeviceDiscoveryService::~DeviceDiscoveryService() { stop(); }

bool DeviceDiscoveryService::begin(std::string device_id, std::string hostname, uint16_t api_port, bool secure) {
    if (running_.load()) return true;
    if (device_id.empty() || hostname.empty() || api_port == 0) return false;
    device_id_ = std::move(device_id);
    hostname_ = std::move(hostname);
    instance_ = "HomeGuard-S3 " + device_id_.substr(device_id_.find_last_of('-') + 1);
    hostname_local_ = hostname_ + ".local";
    api_port_ = api_port;
    secure_ = secure;
    port_txt_ = std::to_string(api_port_);

    if (mdns_init() != ESP_OK) {
        ESP_LOGE(tag, "mdns_init failed");
        return false;
    }
    if (mdns_hostname_set(hostname_.c_str()) != ESP_OK || mdns_instance_name_set(instance_.c_str()) != ESP_OK) {
        ESP_LOGE(tag, "mDNS identity configuration failed");
        mdns_free();
        return false;
    }
    const mdns_txt_item_t txt[] = {
        {"id", device_id_.c_str()},
        {"api", "1"},
        {"tls", secure_ ? "1" : "0"},
        {"model", "HomeGuard-S3"},
        {"host", hostname_local_.c_str()},
        {"port", port_txt_.c_str()},
    };
    if (mdns_service_add(instance_.c_str(), "_homeguard", "_tcp", api_port_, txt, sizeof(txt) / sizeof(txt[0])) != ESP_OK) {
        ESP_LOGE(tag, "mDNS service registration failed");
        mdns_free();
        return false;
    }

    running_.store(true);
    if (xTaskCreate(&DeviceDiscoveryService::udp_task_entry, "hg_discovery", 4096, this, 4, nullptr) != pdPASS) {
        running_.store(false);
        mdns_free();
        ESP_LOGE(tag, "UDP discovery task creation failed");
        return false;
    }
    ESP_LOGI(tag, "advertising %s.local _homeguard._tcp:%u", hostname_.c_str(), api_port_);
    return true;
}

void DeviceDiscoveryService::stop() {
    if (!running_.exchange(false)) return;
    if (socket_ >= 0) {
        shutdown(socket_, SHUT_RDWR);
        close(socket_);
        socket_ = -1;
    }
    mdns_service_remove("_homeguard", "_tcp");
    mdns_free();
}

void DeviceDiscoveryService::udp_task_entry(void* context) {
    static_cast<DeviceDiscoveryService*>(context)->udp_task();
    vTaskDelete(nullptr);
}

void DeviceDiscoveryService::udp_task() {
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socket_ < 0) {
        ESP_LOGE(tag, "socket failed: %d", errno);
        running_.store(false);
        return;
    }
    int reuse = 1;
    setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    timeval timeout{receive_timeout_ms / 1000, (receive_timeout_ms % 1000) * 1000};
    setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    sockaddr_in listen_address{};
    listen_address.sin_family = AF_INET;
    listen_address.sin_port = htons(hg::discovery_udp_port);
    listen_address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(socket_, reinterpret_cast<sockaddr*>(&listen_address), sizeof(listen_address)) != 0) {
        ESP_LOGE(tag, "bind UDP/%u failed: %d", hg::discovery_udp_port, errno);
        close(socket_);
        socket_ = -1;
        running_.store(false);
        return;
    }

    std::array<char, 256> buffer{};
    while (running_.load()) {
        sockaddr_in source{};
        socklen_t source_length = sizeof(source);
        const int received = recvfrom(socket_, buffer.data(), buffer.size(), 0,
                                      reinterpret_cast<sockaddr*>(&source), &source_length);
        if (received <= 0) continue;
        if (!hg::is_discovery_request(std::string_view(buffer.data(), static_cast<size_t>(received)))) continue;

        hg::DiscoveryRecord record{};
        record.device_id = device_id_;
        record.hostname = hostname_;
        record.port = api_port_;
        record.transport = transport_.load();
        record.secure = secure_;
        record.api_version = 1;
        const auto response = hg::make_discovery_response(record);
        sendto(socket_, response.data(), response.size(), 0,
               reinterpret_cast<const sockaddr*>(&source), source_length);
    }
    if (socket_ >= 0) close(socket_);
    socket_ = -1;
}
