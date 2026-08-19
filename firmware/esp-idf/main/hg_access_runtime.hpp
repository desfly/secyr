#pragma once

#include "homeguard/access_control.hpp"

#include <atomic>

namespace homeguard::idf::access_runtime {

inline std::atomic_bool g_bootstrap_allowed{false};

inline void set_bootstrap_allowed(bool allowed) noexcept {
    g_bootstrap_allowed.store(allowed, std::memory_order_release);
}

inline bool bootstrap_allowed() noexcept {
    return g_bootstrap_allowed.load(std::memory_order_acquire);
}

inline bool setup_required(const homeguard::AccessControl& access) noexcept {
    return bootstrap_allowed() && access.user_count() == 0U;
}

inline void lock_bootstrap() noexcept {
    set_bootstrap_allowed(false);
}

}  // namespace homeguard::idf::access_runtime
