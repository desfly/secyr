#pragma once

#include <cstdint>

namespace hg {

struct ResetSequenceStep {
    std::uint8_t count{0};
    bool trigger_factory_reset{false};
};

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
