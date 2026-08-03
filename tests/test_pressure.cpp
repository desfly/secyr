#include "test_framework.hpp"
#include "homeguard/pressure.hpp"
#include "homeguard/power_monitor.hpp"
#include <limits>
void test_pressure(){
 hg::PressureEvaluator p({true,1.0F,4.0F,0.2F});
 CHECK(p.update(2.0F)==hg::PressureState::Normal);
 CHECK(p.update(0.9F)==hg::PressureState::Low);
 CHECK(p.update(1.1F)==hg::PressureState::Low);
 CHECK(p.update(1.2F)==hg::PressureState::Normal);
 CHECK(p.update(4.1F)==hg::PressureState::High);
 CHECK(p.update(3.9F)==hg::PressureState::High);
 CHECK(p.update(3.79F)==hg::PressureState::Normal);
 CHECK(p.update(std::numeric_limits<float>::quiet_NaN())==hg::PressureState::SensorFault);
 hg::PowerMonitor power; CHECK(power.update(true,8.0F)==hg::PowerState::Mains); CHECK(power.update(false,11.0F)==hg::PowerState::Battery); CHECK(power.update(false,10.0F)==hg::PowerState::LowBattery); CHECK(power.update(false,9.0F)==hg::PowerState::Critical);
}
