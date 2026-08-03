#include "homeguard/boot_self_test.hpp"
namespace hg {
HealthState BootSelfTest::run(const SelfTestInputs&i,HealthMonitor&h,uint64_t now){h.set(Component::Esp,HealthState::Ok,now);h.set(Component::Nvs,i.nvs?HealthState::Ok:HealthState::Failed,now);h.set(Component::Rtc,i.rtc?HealthState::Ok:HealthState::Degraded,now);h.set(Component::Adc1,i.adc1?HealthState::Ok:HealthState::Failed,now);h.set(Component::Adc2,i.adc2?HealthState::Ok:HealthState::Failed,now);h.set(Component::W5500,i.w5500?HealthState::Ok:HealthState::Degraded,now);h.set(Component::Wifi,i.wifi?HealthState::Ok:HealthState::Degraded,now);h.set(Component::Inputs,i.inputs?HealthState::Ok:HealthState::Failed,now);h.set(Component::Outputs,i.outputs?HealthState::Ok:HealthState::Failed,now);return h.overall();}
}
