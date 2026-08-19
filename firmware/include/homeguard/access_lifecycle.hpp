#pragma once

#include <cstddef>

namespace hg {

enum class AccessLifecycleState {
    SetupRequired,
    LoginRequired,
    Authenticated,
};

constexpr AccessLifecycleState access_lifecycle_state(
    std::size_t persisted_user_count,
    bool access_store_valid,
    bool authenticated) noexcept {
    if (!access_store_valid) {
        // Corrupt/unreadable persisted access must fail closed. The caller may
        // pass persisted_user_count=0 when the store cannot be read; it still
        // must never silently reopen passwordless bootstrap.
        return AccessLifecycleState::LoginRequired;
    }
    if (persisted_user_count == 0U) {
        return AccessLifecycleState::SetupRequired;
    }
    return authenticated
        ? AccessLifecycleState::Authenticated
        : AccessLifecycleState::LoginRequired;
}

constexpr bool bootstrap_allowed(AccessLifecycleState state) noexcept {
    return state == AccessLifecycleState::SetupRequired;
}

constexpr bool setup_surface_allowed(AccessLifecycleState state) noexcept {
    return state == AccessLifecycleState::SetupRequired;
}

constexpr bool protected_system_state_allowed(AccessLifecycleState state) noexcept {
    return state == AccessLifecycleState::Authenticated;
}

constexpr bool protected_api_allowed(AccessLifecycleState state) noexcept {
    return state == AccessLifecycleState::Authenticated;
}

}  // namespace hg
