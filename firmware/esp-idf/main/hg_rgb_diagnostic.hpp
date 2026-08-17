#pragma once

#include "esp_err.h"

#include <cstdint>

namespace homeguard::idf {

class RgbDiagnostic {
public:
    // Direct WS2812 control used by diagnostics and the physical factory-reset indication.
    // Public API uses RGB order; the driver converts it to the LED's GRB wire order.
    static esp_err_t set_color(int gpio, std::uint8_t red, std::uint8_t green, std::uint8_t blue);
    static esp_err_t off(int gpio);

    // Sends white to the addressable RGB LED, holds it for duration_ms, then turns it off.
    static esp_err_t test_white(int gpio, unsigned duration_ms = 3000U);

private:
    static bool supported_gpio(int gpio);
};

}  // namespace homeguard::idf
