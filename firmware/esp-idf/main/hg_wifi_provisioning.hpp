#pragma once

#include "esp_err.h"

#include <array>
#include <cstdint>

namespace homeguard::idf {

class WifiProvisioningRuntime {
public:
    esp_err_t start(bool provisioning_required);
    esp_err_t connect_station(const char* ssid, const char* password);

    [[nodiscard]] bool started() const { return started_; }
    [[nodiscard]] bool station_connecting() const { return station_connecting_; }
    [[nodiscard]] const char* ssid() const { return ssid_.data(); }
    [[nodiscard]] const char* ip_address() const { return ip_address_.data(); }

private:
    std::array<char, 33> ssid_{};
    std::array<char, 16> ip_address_{};
    bool started_{};
    bool station_connecting_{};
};

}  // namespace homeguard::idf
