#include "hg_mcp23017_output_backend.hpp"

namespace homeguard::idf {
namespace {

constexpr int kColdOpen = 2;
constexpr int kColdClose = 3;
constexpr int kHotOpen = 4;
constexpr int kHotClose = 5;

constexpr std::uint8_t bit_for(int channel) noexcept
{
    return static_cast<std::uint8_t>(1U << static_cast<unsigned>(channel));
}

}  // namespace

void Mcp23017OutputBackend::attach(Mcp23017* expander) noexcept
{
    expander_ = expander;
    configured_.fill(false);
    shadow_ = 0;
}

bool Mcp23017OutputBackend::valid_channel(int channel) noexcept
{
    return channel >= 0 && channel < static_cast<int>(8);
}

std::uint8_t Mcp23017OutputBackend::interlocked_value(
    std::uint8_t current,
    int channel,
    bool level) noexcept
{
    if (!valid_channel(channel)) {
        return current;
    }

    auto next = current;
    const auto mask = bit_for(channel);
    if (level) {
        // OPEN/CLOSE for one valve are mutually exclusive in one atomic OLAT
        // update. Firmware interlock complements, but never replaces, the
        // required hardware relay/H-bridge interlock.
        if (channel == kColdOpen) next &= static_cast<std::uint8_t>(~bit_for(kColdClose));
        if (channel == kColdClose) next &= static_cast<std::uint8_t>(~bit_for(kColdOpen));
        if (channel == kHotOpen) next &= static_cast<std::uint8_t>(~bit_for(kHotClose));
        if (channel == kHotClose) next &= static_cast<std::uint8_t>(~bit_for(kHotOpen));
        next |= mask;
    } else {
        next &= static_cast<std::uint8_t>(~mask);
    }
    return next;
}

bool Mcp23017OutputBackend::commit(std::uint8_t value)
{
    if (expander_ == nullptr || !expander_->ready()) {
        return false;
    }

    if (expander_->write_outputs(value) != ESP_OK) {
        // Do not trust the software shadow after a bus failure. Attempt one
        // explicit all-OFF write and keep the software view fail-closed.
        (void)expander_->force_safe_outputs();
        shadow_ = 0;
        return false;
    }

    shadow_ = value;
    return true;
}

bool Mcp23017OutputBackend::configure_output(int channel, bool initial_level)
{
    if (!valid_channel(channel) || expander_ == nullptr || !expander_->ready()) {
        return false;
    }

    configured_[static_cast<std::size_t>(channel)] = true;
    if (!write_output(channel, initial_level)) {
        configured_[static_cast<std::size_t>(channel)] = false;
        return false;
    }
    return true;
}

bool Mcp23017OutputBackend::write_output(int channel, bool level)
{
    if (!valid_channel(channel) || expander_ == nullptr || !expander_->ready() ||
        !configured_[static_cast<std::size_t>(channel)]) {
        return false;
    }

    return commit(interlocked_value(shadow_, channel, level));
}

bool Mcp23017OutputBackend::read_inputs(std::uint8_t* value)
{
    if (value == nullptr || expander_ == nullptr || !expander_->ready()) {
        return false;
    }
    return expander_->read_inputs(value) == ESP_OK;
}

}  // namespace homeguard::idf
