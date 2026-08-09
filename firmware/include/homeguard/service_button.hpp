#pragma once

#include <cstdint>

namespace hg {

enum class ServiceButtonEvent : uint8_t {
    None,
    RebootRequested,
    FactoryResetRequested,
};

struct ServiceButtonConfig {
    uint32_t debounce_ms{40};
    uint32_t factory_reset_hold_ms{5000};
};

class ServiceButton {
public:
    explicit ServiceButton(ServiceButtonConfig config = {});
    ServiceButtonEvent update(bool raw_pressed, uint64_t now_ms);
    void reset();
    [[nodiscard]] bool pressed() const { return stable_pressed_; }
    [[nodiscard]] uint64_t held_ms(uint64_t now_ms) const;
private:
    ServiceButtonConfig config_{};
    bool raw_pressed_{};
    bool stable_pressed_{};
    bool reset_emitted_{};
    uint64_t raw_changed_at_{};
    uint64_t pressed_at_{};
};

}  // namespace hg
