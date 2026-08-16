#include "hg_rgb_diagnostic.hpp"

namespace homeguard::idf {

bool RgbDiagnostic::supported_gpio(int gpio) {
    return gpio == 38 || gpio == 48;
}

esp_err_t RgbDiagnostic::test_white(int gpio, unsigned duration_ms) {
    return supported_gpio(gpio) && duration_ms > 0U && duration_ms <= 5000U
        ? ESP_OK
        : ESP_ERR_INVALID_ARG;
}

esp_err_t RgbDiagnostic::factory_reset_sequence(int gpio) {
    return supported_gpio(gpio) ? ESP_OK : ESP_ERR_INVALID_ARG;
}

}  // namespace homeguard::idf
