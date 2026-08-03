#include "homeguard/health_monitor.hpp"
namespace hg {
void HealthMonitor::set(Component c, HealthState s, uint64_t now) { auto& e=entries_[static_cast<size_t>(c)]; if(e.state!=s)e.changed_at_ms=now; e.state=s; if(s==HealthState::Ok)e.consecutive_failures=0; }
void HealthMonitor::report(Component c,bool success,uint64_t now,uint32_t degrade_after,uint32_t fail_after){ auto& e=entries_[static_cast<size_t>(c)]; if(success){set(c,HealthState::Ok,now);return;} ++e.consecutive_failures; set(c,e.consecutive_failures>=fail_after?HealthState::Failed:(e.consecutive_failures>=degrade_after?HealthState::Degraded:HealthState::Unknown),now); }
HealthEntry HealthMonitor::get(Component c) const { return entries_[static_cast<size_t>(c)]; }
HealthState HealthMonitor::overall() const { bool unknown=false,degraded=false; for(const auto&e:entries_){if(e.state==HealthState::Failed)return HealthState::Failed;if(e.state==HealthState::Degraded)degraded=true;if(e.state==HealthState::Unknown)unknown=true;} return degraded?HealthState::Degraded:(unknown?HealthState::Unknown:HealthState::Ok); }
uint32_t HealthMonitor::failed_count() const { uint32_t n=0;for(const auto&e:entries_)n+=e.state==HealthState::Failed;return n; }
uint32_t HealthMonitor::degraded_count() const { uint32_t n=0;for(const auto&e:entries_)n+=e.state==HealthState::Degraded;return n; }
std::string_view component_name(Component c){constexpr std::array<std::string_view,9> n={"esp","nvs","rtc","adc1","adc2","w5500","wifi","inputs","outputs"};return n[static_cast<size_t>(c)];}
}
