#include "test_framework.hpp"
#include "homeguard/ble_remote.hpp"

#include <array>
#include <cstdint>
#include <string_view>

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
    const auto remote = identity(1);
    const auto stranger = identity(50);

    hg::BleRemotePermissions permissions{};
    permissions.arm_home = true;
    permissions.disarm = true;
    permissions.lock_pulse = true;
    permissions.panic = true;

    CHECK(registry.bind(remote, "user-1", "Main remote", permissions));
    CHECK(registry.count() == 1U);
    const auto* binding = registry.binding_for(remote);
    CHECK(binding != nullptr);
    CHECK(std::string_view(binding->owner_user_id.data()) == "user-1");
    CHECK(std::string_view(binding->name.data()) == "Main remote");

    auto result = registry.authorize({remote, hg::BleRemoteAction::LockPulse, 1, true});
    CHECK(result.decision == hg::BleRemoteDecision::Accepted);

    result = registry.authorize({remote, hg::BleRemoteAction::LockPulse, 1, true});
    CHECK(result.decision == hg::BleRemoteDecision::Replay);

    permissions.light = true;
    CHECK(registry.bind(remote, "user-2", "Garage remote", permissions));
    binding = registry.binding_for(remote);
    CHECK(binding != nullptr);
    CHECK(std::string_view(binding->owner_user_id.data()) == "user-2");
    CHECK(std::string_view(binding->name.data()) == "Garage remote");
    CHECK(binding->last_counter == 1U);

    result = registry.authorize({remote, hg::BleRemoteAction::Light, 2, true});
    CHECK(result.decision == hg::BleRemoteDecision::Accepted);

    result = registry.authorize({stranger, hg::BleRemoteAction::Panic, 1, true});
    CHECK(result.decision == hg::BleRemoteDecision::UnknownRemote);

    const auto saved = *binding;
    hg::BleRemoteRegistry restored;
    CHECK(restored.import_binding(saved));
    CHECK(restored.count() == 1U);
    CHECK(std::string_view(restored.binding_for(remote)->owner_user_id.data()) == "user-2");

    CHECK(registry.unbind(remote));
    CHECK(registry.count() == 0U);
    result = registry.authorize({remote, hg::BleRemoteAction::Disarm, 3, true});
    CHECK(result.decision == hg::BleRemoteDecision::UnknownRemote);
}
