#include "homeguard/service_button.hpp"

#include <algorithm>

namespace hg {

ServiceButton::ServiceButton(ServiceButtonConfig config) : config_(config) {
    config_.debounce_ms = std::max(config_.debounce_ms, uint32_t{10});
    config_.factory_reset_hold_ms = std::max(config_.factory_reset_hold_ms, uint32_t{1000});
}

ServiceButtonEvent ServiceButton::update(const bool raw_pressed, const uint64_t now_ms) {
    if (raw_pressed != raw_pressed_) {
        raw_pressed_ = raw_pressed;
        raw_changed_at_ = now_ms;
    }

    if (stable_pressed_ != raw_pressed_ && now_ms - raw_changed_at_ >= config_.debounce_ms) {
        const bool was_pressed = stable_pressed_;
        const uint64_t press_duration = was_pressed && now_ms >= pressed_at_ ? now_ms - pressed_at_ : 0;
        stable_pressed_ = raw_pressed_;

        if (stable_pressed_) {
            pressed_at_ = now_ms;
            reset_emitted_ = false;
        } else {
            pressed_at_ = 0;
            if (was_pressed && !reset_emitted_ && press_duration < config_.factory_reset_hold_ms) {
                return ServiceButtonEvent::RebootRequested;
            }
        }
    }

    if (!stable_pressed_) return ServiceButtonEvent::None;
    const uint64_t duration = held_ms(now_ms);
    if (!reset_emitted_ && duration >= config_.factory_reset_hold_ms) {
        reset_emitted_ = true;
        return ServiceButtonEvent::FactoryResetRequested;
    }
    return ServiceButtonEvent::None;
}

void ServiceButton::reset() {
    raw_pressed_ = false;
    stable_pressed_ = false;
    reset_emitted_ = false;
    raw_changed_at_ = 0;
    pressed_at_ = 0;
}

uint64_t ServiceButton::held_ms(const uint64_t now_ms) const {
    return stable_pressed_ && now_ms >= pressed_at_ ? now_ms - pressed_at_ : 0;
}

}  // namespace hg
