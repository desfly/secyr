#pragma once
#include "homeguard/config.hpp"
#include <cstdint>
namespace hg {
enum class PressureState : uint8_t { Disabled, Normal, Low, High, SensorFault };
class PressureEvaluator {
public:
 PressureEvaluator(PressureConfig cfg={}) : cfg_(cfg) {}
 void configure(PressureConfig cfg) { cfg_=cfg; }
 PressureState update(float value);
 [[nodiscard]] PressureState state() const { return state_; }
private: PressureConfig cfg_{}; PressureState state_{PressureState::Normal};
};
}
