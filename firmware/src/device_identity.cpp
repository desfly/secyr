#include "homeguard/device_identity.hpp"
#include <array>
#include <cstdio>

namespace hg {
namespace {
std::string suffix(const std::array<uint8_t, 6>& mac) {
    std::array<char, 7> out{};
    std::snprintf(out.data(), out.size(), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    return out.data();
}
}
std::string DeviceIdentity::device_id() const { return "HG-S3-" + suffix(mac_); }
std::string DeviceIdentity::hostname() const {
    auto value = suffix(mac_);
    for (auto& c : value) if (c >= 'A' && c <= 'F') c = static_cast<char>(c - 'A' + 'a');
    return "homeguard-s3-" + value;
}
std::string DeviceIdentity::service_instance() const { return "HomeGuard-S3 " + suffix(mac_); }
}
