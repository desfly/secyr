#pragma once

#include "driver/gpio.h"

namespace homeguard::board {

inline constexpr const char* kBoardName = "HW-678 V0.0.0";
inline constexpr const char* kModuleName = "ESP32-S3-WROOM-1-N16R8";

inline constexpr gpio_num_t kI2cSda = GPIO_NUM_4;
inline constexpr gpio_num_t kI2cScl = GPIO_NUM_5;
inline constexpr gpio_num_t kOneWire = GPIO_NUM_6;
inline constexpr gpio_num_t kRtcInterrupt = GPIO_NUM_7;

inline constexpr gpio_num_t kW5500Reset = GPIO_NUM_8;
inline constexpr gpio_num_t kW5500Interrupt = GPIO_NUM_9;
inline constexpr gpio_num_t kW5500Cs = GPIO_NUM_10;
inline constexpr gpio_num_t kW5500Mosi = GPIO_NUM_11;
inline constexpr gpio_num_t kW5500Sck = GPIO_NUM_12;
inline constexpr gpio_num_t kW5500Miso = GPIO_NUM_13;

inline constexpr gpio_num_t kTamper = GPIO_NUM_14;
inline constexpr gpio_num_t kPowerFail = GPIO_NUM_15;

inline constexpr gpio_num_t kRs485De = GPIO_NUM_16;
inline constexpr gpio_num_t kRs485Tx = GPIO_NUM_17;
inline constexpr gpio_num_t kRs485Rx = GPIO_NUM_18;

inline constexpr gpio_num_t kServiceButton = GPIO_NUM_21;

inline constexpr gpio_num_t kSdCs = GPIO_NUM_39;
inline constexpr gpio_num_t kSdSck = GPIO_NUM_40;
inline constexpr gpio_num_t kSdMosi = GPIO_NUM_41;
inline constexpr gpio_num_t kSdMiso = GPIO_NUM_42;

// Internal onboard WS2812 status/reset indicator. GPIO48 is reserved from
// external expansion specifically because it belongs to the board itself.
inline constexpr int kOnboardRgb = 48;

inline constexpr bool is_reserved_gpio(int gpio) noexcept
{
    switch (gpio) {
    case 0:
    case 3:
    case 19:
    case 20:
    case 35:
    case 36:
    case 37:
    case 43:
    case 44:
    case 45:
    case 46:
    case 48:
        return true;
    default:
        return false;
    }
}

}  // namespace homeguard::board
