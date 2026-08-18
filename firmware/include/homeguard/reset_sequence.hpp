#pragma once

#include <cstdint>

namespace hg {

struct ResetSequenceStep {
    std::uint8_t count{0};
    bool trigger_factory_reset{false};
};

// The HomeGuard-S3 board reports the physical RST/EN button as POWERON.
// Both EXT and POWERON therefore count as physical-reset candidates. The
// runtime persistence layer decides whether consecutive candidates belong to
// one short reset sequence; software/watchdog/panic resets must not count.
constexpr bool physical_reset_candidate(
    bool external_rst,
    bool poweron_rst) noexcept {
    return external_rst || poweron_rst;
}

constexpr ResetSequenceStep advance_reset_sequence(
    std::uint8_t previous_count,
    bool physical_rst,
    std::uint8_t required_presses = 3U) noexcept {
    if (!physical_rst || required_presses == 0U) return {};
    const auto next = static_cast<std::uint8_t>(previous_count == 0xffU ? 0xffU : previous_count + 1U);
    if (next >= required_presses) return {0U, true};
    return {next, false};
}

}  // namespace hg
