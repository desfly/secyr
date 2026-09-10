#include "test_framework.hpp"
#include "homeguard/ble_remote.hpp"

#include <array>
#include <cstdint>

namespace {
std::array<std::uint8_t, 16> identity(std::uint8_t seed)
{
    std::array<std::uint8_t, 16> value{};
    for (std::size_t index = 0; index < value.size(); ++index) {
        value[index] = static_cast<std::uint8_t>(seed + index);
    }
    return value;
}
}

void test_ble_remote()
{
    hg::BleRemoteRegistry registry;
    const auto owner = identity(1);
    const auto stranger = identity(50);

    hg::BleRemotePermissions permissions{};
    permissions.arm_home = true;
    permissions.disarm = true;
    permissions.lock_pulse = true;
    permissions.panic = true;

    CHECK(registry.bind(owner, permissions));
    CHECK(registry.count() == 1U);

    auto result = registry.authorize({owner, hg::BleRemoteAction::LockPulse, 1, true});
    CHECK(result.decision == hg::BleRemoteDecision::Accepted);

    result = registry.authorize({owner, hg::BleRemoteAction::LockPulse, 1, true});
    CHECK(result.decision == hg::BleRemoteDecision::Replay);

    result = registry.authorize({owner, hg::BleRemoteAction::Light, 2, true});
    CHECK(result.decision == hg::BleRemoteDecision::ActionDenied);

    result = registry.authorize({stranger, hg::BleRemoteAction::Panic, 1, true});
    CHECK(result.decision == hg::BleRemoteDecision::UnknownRemote);

    result = registry.authorize({owner, hg::BleRemoteAction::Disarm, 2, true});
    CHECK(result.decision == hg::BleRemoteDecision::Accepted);

    CHECK(registry.unbind(owner));
    CHECK(registry.count() == 0U);
    result = registry.authorize({owner, hg::BleRemoteAction::Disarm, 3, true});
    CHECK(result.decision == hg::BleRemoteDecision::UnknownRemote);
}
