#include "hg_pressure_420ma.hpp"

#include <algorithm>
#include <cmath>

namespace homeguard::idf {

Pressure420Reading decode_pressure_420ma(
    float millivolts,
    const Pressure420Config& config)
{
    Pressure420Reading out{};
    out.millivolts = millivolts;

    if (!std::isfinite(millivolts) ||
        !std::isfinite(config.shunt_ohms) ||
        config.shunt_ohms <= 0.0F ||
        config.max_current_ma <= config.min_current_ma ||
        config.max_bar <= config.min_bar) {
        return out;
    }

    out.current_ma = millivolts / config.shunt_ohms;

    constexpr float kFaultMarginMa = 0.5F;
    if (out.current_ma < (config.min_current_ma - kFaultMarginMa) ||
        out.current_ma > (config.max_current_ma + kFaultMarginMa)) {
        return out;
    }

    const float normalized =
        (out.current_ma - config.min_current_ma) /
        (config.max_current_ma - config.min_current_ma);

    out.pressure_bar =
        config.min_bar +
        std::clamp(normalized, 0.0F, 1.0F) *
            (config.max_bar - config.min_bar);
    out.valid = true;
    return out;
}

}  // namespace homeguard::idf
