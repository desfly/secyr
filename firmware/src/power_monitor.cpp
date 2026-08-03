#include "homeguard/power_monitor.hpp"
namespace hg { PowerState PowerMonitor::update(bool mains,float v){if(mains)return state_=PowerState::Mains;if(v<9.6F)return state_=PowerState::Critical;if(v<10.5F)return state_=PowerState::LowBattery;return state_=PowerState::Battery;} }
