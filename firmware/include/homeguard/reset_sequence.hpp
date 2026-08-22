#pragma once

#include <cstdint>

namespace hg {

// Approved HomeGuard-S3 physical RST/EN factory-reset contract.
inline constexpr std::uint8_t kFactoryResetRequiredRstPresses = 3U;
inline constexpr std::uint32_t kFactoryResetStepWhiteMs = 1500U;
inline constexpr std::uint32_t kFactoryResetSequenceWindowMs = 5000U;
inline constexpr std::uint32_t kFactoryResetSuccessRedMs = 5000U;

struct ResetSequenceStep {
    std::uint8_t count{0};
    bool trigger_factory_reset{false};
};

// Hardware evidence from HW-678 shows that the physical RST/EN button is
// reported as POWERON and that RTC_NOINIT does not survive that reset. A small
// persistent boot marker is therefore used as the baseline: the very first
// POWERON without a marker is not a gesture step; later POWERON resets can be.
constexpr bool reset_press_detected(
    bool boot_marker_was_valid,
    bool external_rst,
    bool poweron_rst) noexcept {
    return external_rst || (poweron_rst && boot_marker_was_valid);
}

constexpr ResetSequenceStep advance_reset_sequence(
    std::uint8_t previous_count,
    bool physical_rst,
    std::uint8_t required_presses = kFactoryResetRequiredRstPresses) noexcept {
    if (!physical_rst || required_presses == 0U) return {0U, false};

    const auto next = static_cast<std::uint8_t>(
        previous_count == 0xffU ? 0xffU : previous_count + 1U);
    if (next >= required_presses) return {0U, true};
    return {next, false};
}

// Generic confirmed-hold helpers are still shared by the isolated NVS-recovery
// mode. They are intentionally not used by the normal physical RST/EN path.
struct ResetGestureStep {
    std::uint8_t count{0};
    bool trigger_factory_reset{false};
};

constexpr ResetGestureStep advance_confirmed_hold(
    std::uint8_t previous_count,
    bool hold_confirmed,
    std::uint8_t required_holds = 3U) noexcept {
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
