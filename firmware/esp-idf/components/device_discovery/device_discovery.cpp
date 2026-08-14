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
    mdns_started_ = false;

    const auto mdns_error = mdns_init();
    if (mdns_error == ESP_OK) {
        const auto hostname_error = mdns_hostname_set(hostname_.c_str());
        const auto instance_error = mdns_instance_name_set(instance_.c_str());
        if (hostname_error == ESP_OK && instance_error == ESP_OK) {
            mdns_txt_item_t txt[] = {
                {"id", device_id_.c_str()},
                {"api", "1"},
                {"tls", secure_ ? "1" : "0"},
                {"model", "HomeGuard-S3"},
                {"host", hostname_local_.c_str()},
                {"port", port_txt_.c_str()},
            };
            const auto service_error = mdns_service_add(
                instance_.c_str(), "_homeguard", "_tcp", api_port_, txt, sizeof(txt) / sizeof(txt[0]));
            if (service_error == ESP_OK) {
                mdns_started_ = true;
                ESP_LOGI(tag, "mDNS advertising %s.local _homeguard._tcp:%u", hostname_.c_str(), api_port_);
            } else {
                ESP_LOGW(tag, "mDNS service registration failed (%s); UDP discovery remains enabled", esp_err_to_name(service_error));
                mdns_free();
            }
        } else {
            ESP_LOGW(tag, "mDNS identity setup failed; UDP discovery remains enabled");
            mdns_free();
        }
    } else {
        ESP_LOGW(tag, "mdns_init failed (%s); UDP discovery remains enabled", esp_err_to_name(mdns_error));
    }

    running_.store(true);
    if (xTaskCreate(&DeviceDiscoveryService::udp_task_entry, "hg_discovery", 4096, this, 4, nullptr) != pdPASS) {
        running_.store(false);
        if (mdns_started_) {
            mdns_service_remove("_homeguard", "_tcp");
            mdns_free();
            mdns_started_ = false;
        }
        ESP_LOGE(tag, "UDP discovery task creation failed");
        return false;
    }
    ESP_LOGI(tag, "UDP discovery responder starting on port %u for %s", hg::discovery_udp_port, device_id_.c_str());
    return true;
}

void DeviceDiscoveryService::stop() {
    if (!running_.exchange(false)) return;
    if (socket_ >= 0) {
        shutdown(socket_, SHUT_RDWR);
        close(socket_);
        socket_ = -1;
    }
    if (mdns_started_) {
        mdns_service_remove("_homeguard", "_tcp");
        mdns_free();
        mdns_started_ = false;
    }
}

void DeviceDiscoveryService::udp_task_entry(void* context) {
    static_cast<DeviceDiscoveryService*>(context)->udp_task();
    vTaskDelete(nullptr);
}

void DeviceDiscoveryService::udp_task() {
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socket_ < 0) {
        ESP_LOGE(tag, "socket failed: errno=%d", errno);
        running_.store(false);
        return;
    }

    int reuse = 1;
    if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
        ESP_LOGW(tag, "SO_REUSEADDR failed: errno=%d", errno);
    }
    timeval timeout{receive_timeout_ms / 1000, (receive_timeout_ms % 1000) * 1000};
    if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
        ESP_LOGW(tag, "SO_RCVTIMEO failed: errno=%d", errno);
    }

    sockaddr_in listen_address{};
    listen_address.sin_family = AF_INET;
    listen_address.sin_port = htons(hg::discovery_udp_port);
    listen_address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(socket_, reinterpret_cast<sockaddr*>(&listen_address), sizeof(listen_address)) != 0) {
        ESP_LOGE(tag, "bind UDP/%u failed: errno=%d", hg::discovery_udp_port, errno);
        close(socket_);
        socket_ = -1;
        running_.store(false);
        return;
    }

    ESP_LOGI(tag, "UDP/%u bound on 0.0.0.0 and ready", hg::discovery_udp_port);
    std::array<char, 256> buffer{};
    while (running_.load()) {
        sockaddr_in source{};
        socklen_t source_length = sizeof(source);
        const int received = recvfrom(socket_, buffer.data(), buffer.size(), 0,
                                      reinterpret_cast<sockaddr*>(&source), &source_length);
        if (received <= 0) continue;

        char source_ip[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &source.sin_addr, source_ip, sizeof(source_ip));
        const auto payload = std::string_view(buffer.data(), static_cast<size_t>(received));
        ESP_LOGI(tag, "UDP discovery RX %d bytes from %s:%u", received, source_ip, ntohs(source.sin_port));

        if (!hg::is_discovery_request(payload)) {
            ESP_LOGW(tag, "UDP discovery ignored unexpected payload from %s:%u", source_ip, ntohs(source.sin_port));
            continue;
        }

        hg::DiscoveryRecord record{};
        record.device_id = device_id_;
        record.hostname = hostname_;
        record.port = api_port_;
        record.transport = transport_.load();
        record.secure = secure_;
        record.api_version = 1;
        const auto response = hg::make_discovery_response(record);
        const int sent = sendto(socket_, response.data(), response.size(), 0,
                                reinterpret_cast<const sockaddr*>(&source), source_length);
        if (sent < 0) {
            ESP_LOGE(tag, "UDP discovery TX to %s:%u failed: errno=%d", source_ip, ntohs(source.sin_port), errno);
        } else {
            ESP_LOGI(tag, "UDP discovery TX %d bytes to %s:%u id=%s", sent, source_ip, ntohs(source.sin_port), device_id_.c_str());
        }
    }

    if (socket_ >= 0) close(socket_);
    socket_ = -1;
    ESP_LOGI(tag, "UDP discovery responder stopped");
}
