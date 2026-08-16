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

bool legacy_direct_outputs_unassigned(const BoardPinMap& pins) {
    return pins.siren == gpio_unassigned &&
        pins.valve1 == gpio_unassigned &&
        pins.valve2 == gpio_unassigned &&
        pins.aux1 == gpio_unassigned &&
        pins.aux2 == gpio_unassigned;
}

bool matches_hw678_fixed_map(const BoardPinMap& pins) {
    // Build-0018/0026 fixed these board routes. Hardware verification may not
    // substitute arbitrary ESP GPIOs for buses that firmware already drives.
    const bool i2c_ok =
        pins.i2c_sda == 4 && pins.i2c_scl == 5;
    const bool w5500_ok =
        pins.w5500_mosi == 11 &&
        pins.w5500_miso == 13 &&
        pins.w5500_sclk == 12 &&
        pins.w5500_cs == 10 &&
        pins.w5500_int == 9 &&
        pins.w5500_rst == 8;
    const bool service_ok =
        pins.service_button == gpio_unassigned || pins.service_button == 21;
    return i2c_ok && w5500_ok && service_ok && legacy_direct_outputs_unassigned(pins);
}
}  // namespace

bool gpio_number_valid(const int gpio) {
    // ESP32-S3 has GPIO0..21 and GPIO26..48. GPIO22..25 do not exist.
    // -1 means intentionally unassigned.
    return gpio == gpio_unassigned ||
        (gpio >= 0 && gpio <= 21) ||
        (gpio >= 26 && gpio <= 48);
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

    if (!all_or_none(std::array<int, 2>{pins.i2c_sda, pins.i2c_scl})) {
        return {PinMapError::IncompleteI2c, pins.i2c_sda, pins.i2c_scl};
    }
    if (!all_or_none(std::array<int, 6>{pins.w5500_mosi, pins.w5500_miso, pins.w5500_sclk,
                                        pins.w5500_cs, pins.w5500_int, pins.w5500_rst})) {
        return {PinMapError::IncompleteW5500, pins.w5500_cs, pins.w5500_int};
    }

    // For the verified HW-678 profile I2C and W5500 are not optional abstract
    // buses: their routes are already fixed by the board wiring. Likewise the
    // actuator outputs are MCP23017 Port A channels, not direct ESP GPIOs.
    if (!matches_hw678_fixed_map(pins)) {
        return {PinMapError::InvalidGpio, gpio_unassigned, gpio_unassigned};
    }

    for (std::size_t i = 0; i < members.size(); ++i) {
        const int first = pins.*members[i];
        if (first == gpio_unassigned) continue;
        for (std::size_t j = i + 1; j < members.size(); ++j) {
            const int second = pins.*members[j];
            if (first == second) return {PinMapError::DuplicateGpio, first, second};
        }
    }
    return {};
}

}  // namespace hg
