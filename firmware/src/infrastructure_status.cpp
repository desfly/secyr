#include "homeguard/infrastructure_status.hpp"

#include <sstream>

namespace homeguard {

std::string infrastructure_status_json(
    const InfrastructureStatus& status)
{
    std::ostringstream output;
    output
        << "{"
        << "\"rtc\":{"
        << "\"ready\":"
        << (status.rtc_ready ? "true" : "false") << ","
        << "\"iso8601\":\"" << status.rtc_iso8601 << "\","
        << "\"temperature_c\":"
        << status.rtc_temperature_c
        << "},"
        << "\"ethernet\":{"
        << "\"initialized\":"
        << (status.ethernet_initialized ? "true" : "false") << ","
        << "\"link_up\":"
        << (status.ethernet_link_up ? "true" : "false") << ","
        << "\"has_ip\":"
        << (status.ethernet_has_ip ? "true" : "false") << ","
        << "\"ipv4\":\"" << status.ethernet_ipv4 << "\""
        << "},"
        << "\"storage\":{"
        << "\"mounted\":"
        << (status.sd_mounted ? "true" : "false") << ","
        << "\"total_bytes\":\""
        << status.sd_total_bytes << "\","
        << "\"free_bytes\":\""
        << status.sd_free_bytes << "\""
        << "},"
        << "\"battery\":{"
        << "\"ready\":"
        << (status.battery_monitor_ready ? "true" : "false") << ","
        << "\"voltage_v\":"
        << status.battery_voltage_v << ","
        << "\"current_a\":"
        << status.battery_current_a << ","
        << "\"power_w\":"
        << status.battery_power_w
        << "}"
        << "}";
    return output.str();
}

}  // namespace homeguard
