#include "hg_gpio_output_backend.hpp"
#include "homeguard/hardware_profile.hpp"
#include "driver/gpio.h"
#include "hg_board_hw678.hpp"

namespace homeguard::idf {

bool GpioOutputBackend::configure_output(int gpio, bool initial_level) {
    if (gpio == hg::direct_light_relay_gpio || gpio == hg::direct_lock_relay_gpio)
        return write_expander(gpio, initial_level);
    if (gpio < 0 || !GPIO_IS_VALID_OUTPUT_GPIO(gpio) || board::is_reserved_gpio(gpio)) return false;
    const auto pin = static_cast<gpio_num_t>(gpio);
    if (gpio_set_level(pin, initial_level ? 1 : 0) != ESP_OK) return false;
    return gpio_set_direction(pin, GPIO_MODE_OUTPUT) == ESP_OK;
}

bool GpioOutputBackend::write_output(int gpio, bool level) {
    if (gpio == hg::direct_light_relay_gpio || gpio == hg::direct_lock_relay_gpio)
        return write_expander(gpio, level);
    if (gpio < 0 || !GPIO_IS_VALID_OUTPUT_GPIO(gpio) || board::is_reserved_gpio(gpio)) return false;
    return gpio_set_level(static_cast<gpio_num_t>(gpio), level ? 1 : 0) == ESP_OK;
}

bool GpioOutputBackend::write_expander(int gpio, bool level) {
    const std::uint8_t mask = gpio == hg::direct_light_relay_gpio ? 0x01U : 0x02U;
    std::lock_guard<std::mutex> lock(expander_mutex_);
    if (expander_ == nullptr || !expander_->ready()) return false;
    // After an uncertain I2C write, never energize either relay again
    // until the device is restarted and the expander is reinitialized.
    if (expander_fault_ && level) return false;
    // A latched fault permits only the full-port OFF value, not writes
    // that might retain another relay in the energized state.
    if (expander_fault_) {
        if (expander_->force_safe_outputs() != ESP_OK) return false;
        expander_shadow_ = 0;
        return true;
    }
    const auto next = static_cast<std::uint8_t>(level ? (expander_shadow_ | mask) : (expander_shadow_ & ~mask));
    if (expander_->write_outputs(next) != ESP_OK) {
        expander_fault_ = true;
        // Best-effort safe reset: a failed I2C transaction cannot guarantee
        // that the physical relay actually switched off.
        if (expander_->force_safe_outputs() == ESP_OK) expander_shadow_ = 0;
        return false;
    }
    expander_shadow_ = next;
    return true;
}

}  // namespace homeguard::idf
