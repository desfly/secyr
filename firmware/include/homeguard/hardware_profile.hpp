#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace hg {

inline constexpr int gpio_unassigned = -1;

struct BoardPinMap {
    int i2c_sda{gpio_unassigned};
    int i2c_scl{gpio_unassigned};
    int w5500_mosi{gpio_unassigned};
    int w5500_miso{gpio_unassigned};
    int w5500_sclk{gpio_unassigned};
    int w5500_cs{gpio_unassigned};
    int w5500_int{gpio_unassigned};
    int w5500_rst{gpio_unassigned};
    int service_button{gpio_unassigned};
    int siren{gpio_unassigned};
    int valve1{gpio_unassigned};
    int valve2{gpio_unassigned};
    int aux1{gpio_unassigned};
    int aux2{gpio_unassigned};
};

enum class PinMapError : uint8_t {
    None,
    InvalidGpio,
    DuplicateGpio,
    IncompleteW5500,
    IncompleteI2c,
};

struct PinMapValidation {
    PinMapError error{PinMapError::None};
    int first{gpio_unassigned};
    int second{gpio_unassigned};
    [[nodiscard]] constexpr bool ok() const { return error == PinMapError::None; }
};

struct HardwareProfile {
    static constexpr std::string_view board = "HW-678 V0.0.0";
    static constexpr std::string_view module = "ESP32-S3-WROOM-1-N16R8";
    static constexpr std::string_view ethernet = "W5500 LAN module";
    static constexpr bool has_usb_uart_ch343p = true;
    static constexpr bool has_usb_c_uart = true;
    static constexpr bool has_usb_c_native = true;
    static constexpr bool has_boot_button = true;
    static constexpr bool has_reset_button = true;
    static constexpr bool has_ws2812 = true;
};

[[nodiscard]] bool gpio_number_valid(int gpio);
[[nodiscard]] bool w5500_assigned(const BoardPinMap& pins);
[[nodiscard]] bool i2c_assigned(const BoardPinMap& pins);
[[nodiscard]] PinMapValidation validate_pin_map(const BoardPinMap& pins);

}  // namespace hg
