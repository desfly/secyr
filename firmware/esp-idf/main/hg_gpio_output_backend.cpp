#include "hg_gpio_output_backend.hpp"
#include <cstdint>
#include "homeguard/hardware_profile.hpp"
#include "driver/gpio.h"
#include "hg_board_hw678.hpp"
#include "esp_timer.h"

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
    const auto next = static_cast<std::uint8_t>(level ? (expander_shadow_ | mask) : (expander_shadow_ & ~mask));
    return apply_expander(next);
}

bool GpioOutputBackend::synchronize_relays(const hg::SystemModel& model) {
    std::lock_guard<std::mutex> lock(expander_mutex_);
    std::uint8_t desired = 0;
    {
        // Read the latest model only after obtaining the shared writer lock.
        const auto state_lock = model.lock();
        const auto light = model.output_snapshot(4);
        const auto latch = model.output_snapshot(5);
        if (light && light->active) desired |= 0x01U;
        if (latch && latch->active) desired |= 0x02U;
    }
    if (expander_ == nullptr || !expander_->ready()) return false;
    if (!expander_fault_ && desired == expander_shadow_) return true;
    return apply_expander(desired);
}

std::uint8_t GpioOutputBackend::relay_levels() {
    std::lock_guard<std::mutex> lock(expander_mutex_);
    return expander_shadow_;
}

std::uint64_t GpioOutputBackend::now_ms() const {
    return clock_ != nullptr ? clock_() : static_cast<std::uint64_t>(esp_timer_get_time() / 1000);
}

bool GpioOutputBackend::enforce_lock_timeout(hg::SystemModel& model) {
    std::lock_guard<std::mutex> lock(expander_mutex_);
    const auto now = now_ms();
    if (lock_deadline_ms_ == 0 || now < lock_deadline_ms_) return true;
    // Local monotonic deadline, independent of Android, HTTP, BLE and MQTT.
    // The output worker invokes this every 20 ms during normal operation.
    const bool updated = model.set_output_active(5, false, now);
    const bool applied = apply_expander(static_cast<std::uint8_t>(expander_shadow_ & ~0x02U));
    return updated && applied;
}

bool GpioOutputBackend::force_safe_relays() {
    std::lock_guard<std::mutex> lock(expander_mutex_);
    // Prevent the background model mirror from undoing an emergency safe-off.
    // Reinitialization at restart is required before energizing either relay.
    expander_fault_ = true;
    if (expander_ == nullptr || !expander_->ready()) return false;
    if (expander_->force_safe_outputs() != ESP_OK) return false;
    expander_shadow_ = 0;
    lock_deadline_ms_ = 0;
    return true;
}

bool GpioOutputBackend::apply_expander(std::uint8_t next) {
    if (expander_ == nullptr || !expander_->ready()) return false;
    // After an uncertain I2C write, never energize either relay again
    // until the device is restarted and the expander is reinitialized.
    // A latched fault permits only the full-port OFF value, not writes
    // that might retain another relay in the energized state.
    if (expander_fault_) {
        // Retry a best-effort safe-off, but never report a successful output
        // operation while the expander is fault-latched. A successful I2C
        // transaction is not proof that the relay contacts are de-energized.
        if (expander_->force_safe_outputs() == ESP_OK) expander_shadow_ = 0;
        return false;
    }
    if (expander_->write_mirrored_outputs(next) != ESP_OK) {
        expander_fault_ = true;
        // Best-effort safe reset: a failed I2C transaction cannot guarantee
        // that the physical relay actually switched off.
        if (expander_->force_safe_outputs() == ESP_OK) expander_shadow_ = 0;
        return false;
    }
    // Repeated ON requests never extend continuous relay energization.
    if ((next & 0x02U) != 0 && (expander_shadow_ & 0x02U) == 0) lock_deadline_ms_ = now_ms() + 5000;
    if ((next & 0x02U) == 0) lock_deadline_ms_ = 0;
    expander_shadow_ = next;
    return true;
}

}  // namespace homeguard::idf
