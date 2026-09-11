#include "test_framework.hpp"
#include "homeguard/ble_remote.hpp"

#include <cstdint>

void test_ble_remote()
{
    hg::BleRemoteRegistry registry;
    const hg::BleRemoteIdentity remote{0, {1, 2, 3, 4, 5, 6}};
    const hg::BleRemoteIdentity stranger{0, {6, 5, 4, 3, 2, 1}};
    const auto permissions =
        hg::BleRemoteRegistry::permission(hg::BleRemoteAction::ArmAway) |
        hg::BleRemoteRegistry::permission(hg::BleRemoteAction::Disarm) |
        hg::BleRemoteRegistry::permission(hg::BleRemoteAction::Panic);

    testfw::expect(registry.bind(remote, hg::BleRemoteProfile::HomeGuardNative, "Keyfob 1", permissions), "bind remote");
    testfw::expect(registry.size() == 1U, "remote registry size");

    auto result = registry.accept({stranger, hg::BleRemoteAction::ArmAway, true, 1});
    testfw::expect(result.decision == hg::BleRemoteDecision::UnknownRemote, "unknown remote denied");

    result = registry.accept({remote, hg::BleRemoteAction::LockPulse, true, 1});
    testfw::expect(result.decision == hg::BleRemoteDecision::PermissionDenied, "ungranted action denied");

    result = registry.accept({remote, hg::BleRemoteAction::ArmAway, false, 0});
    testfw::expect(result.decision == hg::BleRemoteDecision::ReplayRejected, "missing replay counter denied");

    result = registry.accept({remote, hg::BleRemoteAction::ArmAway, true, 10});
    testfw::expect(result.decision == hg::BleRemoteDecision::Accepted, "allowed action accepted");
    testfw::expect(result.command != nullptr, "accepted action maps to command");

    result = registry.accept({remote, hg::BleRemoteAction::Disarm, true, 10});
    testfw::expect(result.decision == hg::BleRemoteDecision::ReplayRejected, "duplicate counter denied");

    result = registry.accept({remote, hg::BleRemoteAction::Disarm, true, 9});
    testfw::expect(result.decision == hg::BleRemoteDecision::ReplayRejected, "older counter denied");

    result = registry.accept({remote, hg::BleRemoteAction::Disarm, true, 11});
    testfw::expect(result.decision == hg::BleRemoteDecision::Accepted, "new counter accepted");

    testfw::expect(registry.set_enabled(remote, false), "disable remote");
    result = registry.accept({remote, hg::BleRemoteAction::Panic, true, 12});
    testfw::expect(result.decision == hg::BleRemoteDecision::Disabled, "disabled remote denied");

    testfw::expect(registry.unbind(remote), "unbind remote");
    result = registry.accept({remote, hg::BleRemoteAction::Panic, true, 13});
    testfw::expect(result.decision == hg::BleRemoteDecision::UnknownRemote, "unbound remote denied");
}
