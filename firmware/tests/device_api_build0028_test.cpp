#include "homeguard/device_api_model.hpp"
#include "homeguard/device_command_router.hpp"

#include <cassert>
#include <iostream>

using namespace homeguard;

int main()
{
    DeviceApiState state;
    state.zones = {{
        {"motion", "Motion", "normal", false, false, 1650.0F},
        {"door", "Door", "normal", false, false, 1650.0F},
        {"flood", "Flood", "normal", true, false, 1650.0F},
        {"smoke", "Smoke", "normal", true, false, 1650.0F},
        {"gas", "Gas", "normal", true, false, 1650.0F},
    }};
    state.valves = {{
        {"cold", "closed", false, 0},
        {"hot", "closed", false, 0},
    }};

    DeviceCommandRouter router(state);

    auto arm = router.handle({
        "r1", "android:test",
        "security.arm_away", "", "",
    });
    assert(arm.code == CommandResultCode::Accepted);
    assert(state.security_mode == SecurityMode::ArmedAway);

    auto duplicate = router.handle({
        "r1", "android:test",
        "security.arm_away", "", "",
    });
    assert(duplicate.code == CommandResultCode::Duplicate);

    auto close = router.handle({
        "r2", "android:test",
        "valve.close", "cold", "",
    });
    assert(close.code == CommandResultCode::Accepted);
    assert(state.valves[0].emergency_latched);

    auto blocked = router.handle({
        "r3", "android:test",
        "valve.open", "cold", "",
    });
    assert(blocked.code == CommandResultCode::Rejected);

    auto clear = router.handle({
        "r4", "android:test",
        "valve.clear_latch", "cold", "",
    });
    assert(clear.code == CommandResultCode::Accepted);

    auto open = router.handle({
        "r5", "android:test",
        "valve.open", "cold", "",
    });
    assert(open.code == CommandResultCode::Accepted);

    const auto json = device_state_json(state);
    assert(json.find("\"security_mode\":\"armed_away\"") !=
           std::string::npos);
    assert(json.find("\"zones\"") != std::string::npos);
    assert(json.find("\"valves\"") != std::string::npos);

    std::cout << "Build-0028 device API tests PASS\n";
    return 0;
}
