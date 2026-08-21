#pragma once

#include <cstdint>

namespace hg {

// Physical RST/EN factory-reset contract for HomeGuard-S3.
inline constexpr std::uint8_t kFactoryResetRequiredRstPresses = 3U;
inline constexpr std::uint32_t kFactoryResetWhiteAckMs = 1500U;
inline constexpr std::uint32_t kFactoryResetSequenceWindowMs = 5000U;
inline constexpr std::uint32_t kFactoryResetSuccessRedMs = 5000U;

struct ResetSequenceStep {
    std::uint8_t count{0};
    bool trigger_factory_reset{false};
};

// This board reports its physical RST/EN button as POWERON. A true cold boot
// must not be mistaken for a reset gesture, so POWERON counts only when the
// RTC marker proves that the application was already running before the reset.
constexpr bool reset_press_detected(
    bool rtc_state_was_valid,
    bool external_rst,
    bool poweron_rst) noexcept {
    return external_rst || (poweron_rst && rtc_state_was_valid);
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

}  // namespace hg
