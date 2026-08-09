#pragma once

#include "esp_err.h"
#include "esp_event.h"

#include <array>
#include <cstdint>

namespace homeguard::idf {

class WifiProvisioningRuntime {
public:
    esp_err_t start(bool provisioning_required);
    esp_err_t connect_station(const char* ssid, const char* password);

    [[nodiscard]] bool started() const { return started_; }
    [[nodiscard]] bool station_connecting() const { return station_connecting_; }
    [[nodiscard]] bool station_connected() const { return station_connected_; }
    [[nodiscard]] const char* ssid() const { return ssid_.data(); }
    [[nodiscard]] const char* ip_address() const { return ip_address_.data(); }
    [[nodiscard]] const char* station_ssid() const { return station_ssid_.data(); }
    [[nodiscard]] const char* station_ip_address() const { return station_ip_address_.data(); }

private:
    static void network_event_handler(void* arg,
                                      esp_event_base_t event_base,
                                      std::int32_t event_id,
                                      void* event_data);

    std::array<char, 33> ssid_{};
    std::array<char, 16> ip_address_{};
    std::array<char, 33> station_ssid_{};
    std::array<char, 16> station_ip_address_{};
    bool started_{};
    bool station_connecting_{};
    bool station_connected_{};
};

}  // namespace homeguard::idf
