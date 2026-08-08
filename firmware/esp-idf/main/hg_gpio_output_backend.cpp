#include "hg_gpio_output_backend.hpp"

#include "driver/gpio.h"

namespace homeguard::idf {

bool GpioOutputBackend::configure_output(int gpio, bool initial_level) {
    const auto pin = static_cast<gpio_num_t>(gpio);
    if (gpio_set_level(pin, initial_level ? 1 : 0) != ESP_OK) return false;
    return gpio_set_direction(pin, GPIO_MODE_OUTPUT) == ESP_OK;
}

bool GpioOutputBackend::write_output(int gpio, bool level) {
    return gpio_set_level(static_cast<gpio_num_t>(gpio), level ? 1 : 0) == ESP_OK;
}

}  // namespace homeguard::idf
