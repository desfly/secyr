#include "homeguard/access_lifecycle.hpp"

using hg::AccessLifecycleState;

static_assert(hg::access_lifecycle_state(0U, true, false) == AccessLifecycleState::SetupRequired);
static_assert(hg::access_lifecycle_state(1U, true, false) == AccessLifecycleState::LoginRequired);
static_assert(hg::access_lifecycle_state(1U, true, true) == AccessLifecycleState::Authenticated);

// Corrupt/unreadable access storage must never silently reopen bootstrap.
static_assert(hg::access_lifecycle_state(0U, false, false) == AccessLifecycleState::LoginRequired);
static_assert(!hg::bootstrap_allowed(AccessLifecycleState::LoginRequired));
static_assert(!hg::bootstrap_allowed(AccessLifecycleState::Authenticated));
static_assert(hg::bootstrap_allowed(AccessLifecycleState::SetupRequired));

// Unauthenticated clients never receive protected controller state.
static_assert(!hg::protected_system_state_allowed(AccessLifecycleState::SetupRequired));
static_assert(!hg::protected_system_state_allowed(AccessLifecycleState::LoginRequired));
static_assert(hg::protected_system_state_allowed(AccessLifecycleState::Authenticated));

static_assert(!hg::protected_api_allowed(AccessLifecycleState::SetupRequired));
static_assert(!hg::protected_api_allowed(AccessLifecycleState::LoginRequired));
static_assert(hg::protected_api_allowed(AccessLifecycleState::Authenticated));
