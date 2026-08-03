#pragma once
#include "homeguard/health_monitor.hpp"
#include <array>
#include <cstdint>
namespace hg {
struct SelfTestInputs { bool nvs{}; bool rtc{}; bool adc1{}; bool adc2{}; bool w5500{}; bool wifi{}; bool inputs{}; bool outputs{}; };
class BootSelfTest { public: static HealthState run(const SelfTestInputs& inputs, HealthMonitor& health, uint64_t now_ms); };
}
