#include "homeguard/infrastructure_status.hpp"

#include <cassert>
#include <iostream>

using namespace homeguard;

int main()
{
    InfrastructureStatus status;
    status.rtc_ready = true;
    status.rtc_iso8601 = "2026-08-03T15:00:00+03:00";
    status.rtc_temperature_c = 29.25F;

    status.ethernet_initialized = true;
    status.ethernet_link_up = true;
    status.ethernet_has_ip = true;
    status.ethernet_ipv4 = "192.168.1.50";

    status.sd_mounted = true;
    status.sd_total_bytes = 32000000000ULL;
    status.sd_free_bytes = 30000000000ULL;

    status.battery_monitor_ready = true;
    status.battery_voltage_v = 12.4F;
    status.battery_current_a = -0.3F;
    status.battery_power_w = -3.72F;

    const auto json =
        infrastructure_status_json(status);

    assert(json.find("\"ready\":true") !=
           std::string::npos);
    assert(json.find("192.168.1.50") !=
           std::string::npos);
    assert(json.find("\"32000000000\"") !=
           std::string::npos);
    assert(json.find("\"power_w\":-3.72") !=
           std::string::npos);

    std::cout << "Build-0019 infrastructure status tests PASS\n";
    return 0;
}
