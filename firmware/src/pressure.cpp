#include "homeguard/pressure.hpp"
#include <cmath>
namespace hg {
PressureState PressureEvaluator::update(float value) {
 if(!cfg_.enabled) return state_=PressureState::Disabled;
 if(!std::isfinite(value)||value<0.0F||value>20.0F) return state_=PressureState::SensorFault;
 if(state_==PressureState::Low && value<cfg_.low+cfg_.hysteresis) return state_;
 if(state_==PressureState::High && value>cfg_.high-cfg_.hysteresis) return state_;
 if(value<cfg_.low) return state_=PressureState::Low;
 if(value>cfg_.high) return state_=PressureState::High;
 return state_=PressureState::Normal;
}
}
