#include "homeguard/hardware_profile.hpp"

#include <array>
#include <cstddef>

namespace hg {
namespace {
constexpr std::array<int BoardPinMap::*, 14> members{
    &BoardPinMap::i2c_sda,
    &BoardPinMap::i2c_scl,
    &BoardPinMap::w5500_mosi,
    &BoardPinMap::w5500_miso,
    &BoardPinMap::w5500_sclk,
    &BoardPinMap::w5500_cs,
    &BoardPinMap::w5500_int,
    &BoardPinMap::w5500_rst,
    &BoardPinMap::service_button,
    &BoardPinMap::siren,
    &BoardPinMap::valve1,
    &BoardPinMap::valve2,
    &BoardPinMap::aux1,
    &BoardPinMap::aux2,
};

bool all_or_none(const std::array<int, 2>& values) {
    const bool any = values[0] != gpio_unassigned || values[1] != gpio_unassigned;
    const bool all = values[0] != gpio_unassigned && values[1] != gpio_unassigned;
    return !any || all;
}

bool all_or_none(const std::array<int, 6>& values) {
    bool any = false;
    bool all = true;
    for (const int value : values) {
        any = any || value != gpio_unassigned;
        all = all && value != gpio_unassigned;
    }
    return !any || all;
}
}  // namespace

bool gpio_number_valid(const int gpio) {
    // -1 means intentionally unassigned. ESP32-S3 exposes GPIO 0..48, but the
    // final PCB review must additionally exclude pins consumed by board wiring.
    return gpio == gpio_unassigned || (gpio >= 0 && gpio <= 48);
}

bool w5500_assigned(const BoardPinMap& pins) {
    return pins.w5500_mosi != gpio_unassigned && pins.w5500_miso != gpio_unassigned &&
        pins.w5500_sclk != gpio_unassigned && pins.w5500_cs != gpio_unassigned &&
        pins.w5500_int != gpio_unassigned && pins.w5500_rst != gpio_unassigned;
}

bool i2c_assigned(const BoardPinMap& pins) {
    return pins.i2c_sda != gpio_unassigned && pins.i2c_scl != gpio_unassigned;
}

PinMapValidation validate_pin_map(const BoardPinMap& pins) {
    for (const auto member : members) {
        const int value = pins.*member;
        if (!gpio_number_valid(value)) return {PinMapError::InvalidGpio, value, gpio_unassigned};
    }

    for (std::size_t i = 0; i < members.size(); ++i) {
        const int first = pins.*members[i];
        if (first == gpio_unassigned) continue;
        for (std::size_t j = i + 1; j < members.size(); ++j) {
            const int second = pins.*members[j];
            if (first == second) return {PinMapError::DuplicateGpio, first, second};
        }
    }

    if (!all_or_none(std::array<int, 2>{pins.i2c_sda, pins.i2c_scl})) {
        return {PinMapError::IncompleteI2c, pins.i2c_sda, pins.i2c_scl};
    }
    if (!all_or_none(std::array<int, 6>{pins.w5500_mosi, pins.w5500_miso, pins.w5500_sclk,
                                        pins.w5500_cs, pins.w5500_int, pins.w5500_rst})) {
        return {PinMapError::IncompleteW5500, pins.w5500_cs, pins.w5500_int};
    }
    return {};
}

}  // namespace hg
