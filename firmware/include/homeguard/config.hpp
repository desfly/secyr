#pragma once
#include <array>
#include <cstdint>
namespace hg {
struct ZoneConfig { bool enabled{true}; bool normally_closed{true}; uint32_t debounce_ms{80}; };
struct PressureConfig { bool enabled{true}; float low{1.2F}; float high{4.5F}; float hysteresis{0.15F}; };
struct ControllerConfig {
    std::array<ZoneConfig,5> zones{};
    std::array<PressureConfig,2> pressures{};
    uint32_t entry_delay_ms{30000};
    uint32_t exit_delay_ms{30000};
    uint32_t network_debounce_ms{1000};
    uint32_t failover_hold_ms{5000};
    uint32_t request_ttl_ms{120000};
};
}
