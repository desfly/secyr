#pragma once

#include "esp_err.h"

#include <array>
#include <cstdint>

namespace homeguard::idf {

class WifiProvisioningRuntime {
public:
    esp_err_t start(bool provisioning_required);

    [[nodiscard]] bool started() const { return started_; }
    [[nodiscard]] const char* ssid() const { return ssid_.data(); }
    [[nodiscard]] const char* ip_address() const { return ip_address_.data(); }

private:
    std::array<char, 33> ssid_{};
    std::array<char, 16> ip_address_{};
    bool started_{};
};

}  // namespace homeguard::idf
