#pragma once

#include <cstdint>
#include <string>

namespace homeguard {

struct InfrastructureStatus {
    bool rtc_ready{false};
    std::string rtc_iso8601;
    float rtc_temperature_c{0.0F};

    bool ethernet_initialized{false};
    bool ethernet_link_up{false};
    bool ethernet_has_ip{false};
    std::string ethernet_ipv4;

    bool sd_mounted{false};
    std::uint64_t sd_total_bytes{0};
    std::uint64_t sd_free_bytes{0};

    bool battery_monitor_ready{false};
    float battery_voltage_v{0.0F};
    float battery_current_a{0.0F};
    float battery_power_w{0.0F};
};

std::string infrastructure_status_json(
    const InfrastructureStatus& status);

}  // namespace homeguard
