#pragma once

#include "esp_err.h"

namespace homeguard::idf {

class RgbDiagnostic {
public:
    // Persistent feedback primitives for the confirmed onboard WS2812.
    static esp_err_t set_white(int gpio);
    static esp_err_t set_red(int gpio);
    static esp_err_t off(int gpio);

    // Sends white, holds it for duration_ms, then turns it off.
    static esp_err_t test_white(int gpio, unsigned duration_ms = 3000U);

private:
    static bool supported_gpio(int gpio);
};

}  // namespace homeguard::idf
