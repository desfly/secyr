#pragma once

#include "homeguard/system_model.hpp"
#include "esp_err.h"

#include <array>
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
    void set_polarity(InputPolarityConfig config) { polarity_ = config; }
    [[nodiscard]] InputPolarityConfig polarity() const { return polarity_; }

private:
    static void task_entry(void* context);
    void run();

    hg::SystemEventBus* bus_{};
    InputPolarityConfig polarity_{};
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
