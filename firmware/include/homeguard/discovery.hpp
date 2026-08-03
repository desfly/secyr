#pragma once
#include "homeguard/types.hpp"
#include <cstdint>
#include <string>
#include <string_view>

namespace hg {
inline constexpr std::string_view discovery_request_v1 = "HG_DISCOVER_V1";
inline constexpr uint16_t discovery_udp_port = 45678;
inline constexpr std::string_view discovery_service_type = "_homeguard._tcp";

struct DiscoveryRecord {
    std::string device_id;
    std::string hostname;
    uint16_t port{443};
    Transport transport{Transport::None};
    bool secure{true};
    bool pairing_required{false};
    uint16_t api_version{1};
};

[[nodiscard]] bool is_discovery_request(std::string_view payload);
[[nodiscard]] std::string make_discovery_response(const DiscoveryRecord& record);
}
