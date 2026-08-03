#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace hg {
class DeviceIdentity {
public:
    explicit DeviceIdentity(std::array<uint8_t, 6> mac) : mac_(mac) {}
    [[nodiscard]] const std::array<uint8_t, 6>& mac() const { return mac_; }
    [[nodiscard]] std::string device_id() const;
    [[nodiscard]] std::string hostname() const;
    [[nodiscard]] std::string service_instance() const;
private:
    std::array<uint8_t, 6> mac_{};
};
}
