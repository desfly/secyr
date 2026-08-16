#pragma once

#include "esp_err.h"

namespace homeguard::idf {

class RgbDiagnostic {
public:
    // Test only the two ESP32-S3 DevKitC-1 RGB candidates documented by Espressif.
    // Sends white to the addressable RGB LED, holds it for duration_ms, then turns it off.
    static esp_err_t test_white(int gpio, unsigned duration_ms = 3000U);

private:
    static bool supported_gpio(int gpio);
};

}  // namespace homeguard::idf
