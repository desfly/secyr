#include "homeguard/service_button.hpp"

#include <algorithm>

namespace hg {

ServiceButton::ServiceButton(ServiceButtonConfig config) : config_(config) {
    config_.factory_reset_hold_ms = std::max(config_.factory_reset_hold_ms, config_.service_hold_ms + 1000U);
}

ServiceButtonEvent ServiceButton::update(const bool raw_pressed, const uint64_t now_ms) {
    if (raw_pressed != raw_pressed_) {
        raw_pressed_ = raw_pressed;
        raw_changed_at_ = now_ms;
    }

    if (stable_pressed_ != raw_pressed_ && now_ms - raw_changed_at_ >= config_.debounce_ms) {
        stable_pressed_ = raw_pressed_;
        if (stable_pressed_) {
            pressed_at_ = now_ms;
            service_emitted_ = false;
            reset_emitted_ = false;
        } else {
            pressed_at_ = 0;
        }
    }

    if (!stable_pressed_) return ServiceButtonEvent::None;
    const uint64_t duration = held_ms(now_ms);
    if (!reset_emitted_ && duration >= config_.factory_reset_hold_ms) {
        reset_emitted_ = true;
        return ServiceButtonEvent::FactoryResetRequested;
    }
    if (!service_emitted_ && duration >= config_.service_hold_ms) {
        service_emitted_ = true;
        return ServiceButtonEvent::ServiceModeRequested;
    }
    return ServiceButtonEvent::None;
}

void ServiceButton::reset() {
    raw_pressed_ = false;
    stable_pressed_ = false;
    service_emitted_ = false;
    reset_emitted_ = false;
    raw_changed_at_ = 0;
    pressed_at_ = 0;
}

uint64_t ServiceButton::held_ms(const uint64_t now_ms) const {
    return stable_pressed_ && now_ms >= pressed_at_ ? now_ms - pressed_at_ : 0;
}

}  // namespace hg
