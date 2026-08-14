#pragma once

#include "homeguard/system_model.hpp"
#include "esp_err.h"

#include <atomic>
#include <cstdint>

namespace homeguard::idf {

enum class InputPolarity : std::uint8_t {
    Unknown = 0,
    ActiveHigh = 1,
    ActiveLow = 2,
};

struct InputPolarityConfig {
    InputPolarity tamper{InputPolarity::Unknown};
    InputPolarity power_fail{InputPolarity::Unknown};
};

class InputRuntime {
public:
    esp_err_t start(hg::SystemEventBus* bus);

    void set_polarity(InputPolarityConfig config) noexcept {
        tamper_polarity_.store(config.tamper, std::memory_order_release);
        power_fail_polarity_.store(config.power_fail, std::memory_order_release);
    }

    [[nodiscard]] InputPolarityConfig polarity() const noexcept {
        return {
            tamper_polarity_.load(std::memory_order_acquire),
            power_fail_polarity_.load(std::memory_order_acquire),
        };
    }

private:
    static void task_entry(void* context);
    void run();

    hg::SystemEventBus* bus_{};
    std::atomic<InputPolarity> tamper_polarity_{InputPolarity::Unknown};
    std::atomic<InputPolarity> power_fail_polarity_{InputPolarity::Unknown};
};

[[nodiscard]] constexpr bool input_is_active(InputPolarity polarity, bool raw_high)
{
    switch (polarity) {
        case InputPolarity::ActiveHigh: return raw_high;
        case InputPolarity::ActiveLow: return !raw_high;
        default: return false;
    }
}

}  // namespace homeguard::idf
