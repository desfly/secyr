#include "homeguard/hardware_api.hpp"
#include "homeguard/hardware_runtime.hpp"

#include <cassert>
#include <iostream>

using namespace homeguard;

int main()
{
    HardwareRuntimeStatus status;
    status.i2c = {
        HardwareModuleState::Ready,
        "GPIO4/GPIO5",
        0,
    };
    status.ads1115_zones = {
        HardwareModuleState::Ready,
        "0x48",
        0,
    };
    status.ads1115_telemetry = {
        HardwareModuleState::Missing,
        "0x49 missing",
        1,
    };
    status.mcp23017 = {
        HardwareModuleState::Ready,
        "outputs off",
        0,
    };
    status.safe_outputs_forced = true;

    const auto response = hardware_status_response(status);
    assert(response.http_status == 200);
    assert(response.body.find("\"i2c\"") != std::string::npos);
    assert(response.body.find("\"state\":\"ready\"") !=
           std::string::npos);
    assert(response.body.find("\"state\":\"missing\"") !=
           std::string::npos);
    assert(response.body.find("\"safe_outputs_forced\":true") !=
           std::string::npos);

    std::cout << "Build-0018 hardware runtime tests PASS\n";
    return 0;
}
