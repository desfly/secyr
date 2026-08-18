#pragma once

#include <cstdint>

namespace hg {

struct ResetSequenceStep {
    std::uint8_t count{0};
    bool trigger_factory_reset{false};
};

// Some ESP32-S3 boards wire the physical RST/EN button such that ESP-IDF
// reports the reset as POWERON rather than EXT. RTC_NOINIT state survives the
// button reset on this hardware, while a true cold boot starts without our
// valid RTC marker. Treat POWERON as a button press only when the marker was
// already valid before this boot.
constexpr bool reset_press_detected(
    bool rtc_state_was_valid,
    bool external_rst,
    bool poweron_rst) noexcept {
    return external_rst || (poweron_rst && rtc_state_was_valid);
}

constexpr ResetSequenceStep advance_reset_sequence(
    std::uint8_t previous_count,
    bool external_rst,
    std::uint8_t required_presses = 3U) noexcept {
    if (!external_rst || required_presses == 0U) return {};
    const auto next = static_cast<std::uint8_t>(previous_count == 0xffU ? 0xffU : previous_count + 1U);
    if (next >= required_presses) return {0U, true};
    return {next, false};
}

}  // namespace hg
