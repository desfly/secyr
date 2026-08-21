#pragma once

#include <cstdint>

namespace hg {

inline constexpr std::uint8_t kFactoryResetRequiredHolds = 3U;
inline constexpr std::uint32_t kFactoryResetHoldMs = 1500U;
inline constexpr std::uint32_t kFactoryResetSequenceTimeoutMs = 5000U;
inline constexpr std::uint32_t kFactoryResetSuccessWhiteMs = 5000U;

struct ResetGestureStep {
    std::uint8_t count{0};
    bool trigger_factory_reset{false};
};

// A gesture step is counted only after the hold threshold has been reached
// and the button is subsequently released. Short presses never advance it.
constexpr ResetGestureStep advance_confirmed_hold(
    std::uint8_t previous_count,
    bool hold_confirmed,
    std::uint8_t required_holds = kFactoryResetRequiredHolds) noexcept {
    if (!hold_confirmed || required_holds == 0U) {
        return {previous_count, false};
    }

    const auto next = static_cast<std::uint8_t>(
        previous_count == 0xffU ? 0xffU : previous_count + 1U);
    if (next >= required_holds) return {0U, true};
    return {next, false};
}

constexpr std::uint8_t expire_reset_gesture(
    std::uint8_t previous_count,
    bool timed_out) noexcept {
    return timed_out ? 0U : previous_count;
}

}  // namespace hg
