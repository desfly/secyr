#pragma once

#include "esp_err.h"

namespace homeguard::idf {

class RgbDiagnostic {
public:
    // Test only the two ESP32-S3 DevKitC-1 RGB candidates documented by Espressif.
    // Sends white to the addressable RGB LED, holds it for duration_ms, then turns it off.
    static esp_err_t test_white(int gpio, unsigned duration_ms = 3000U);

    // Distinct Factory Reset indication: five different colors, one second each,
    // then the LED is turned off before destructive state erase continues.
    static esp_err_t factory_reset_sequence(int gpio);

private:
    static bool supported_gpio(int gpio);
};

}  // namespace homeguard::idf
