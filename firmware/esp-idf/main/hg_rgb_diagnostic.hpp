#pragma once

#include "esp_err.h"

namespace homeguard::idf {

class RgbDiagnostic {
public:
    // Direct feedback controls for the onboard addressable RGB LED.
    static esp_err_t set_red(int gpio);
    static esp_err_t off(int gpio);

    // Sends white to the addressable RGB LED, holds it for duration_ms, then turns it off.
    static esp_err_t test_white(int gpio, unsigned duration_ms = 3000U);

private:
    static bool supported_gpio(int gpio);
};

}  // namespace homeguard::idf
