#include "homeguard/controller.hpp"
#include <cstdlib>
#ifndef ESP_PLATFORM
#include <stdexcept>
#endif
namespace hg {
namespace {
[[noreturn]] void invalid_index(const char* what) {
#ifdef ESP_PLATFORM
 (void)what;
 std::abort();
#else
 throw std::out_of_range(what);
#endif
}
}
Controller::Controller(ControllerConfig c):config_(c),network_(c.network_debounce_ms,c.failover_hold_ms){for(size_t i=0;i<zones_.size();++i)zones_[i].configure(c.zones[i]);for(size_t i=0;i<pressures_.size();++i)pressures_[i].configure(c.pressures[i]);events_.append(0,Severity::Info,1,"controller initialized");}
void Controller::enter_alarm(uint64_t now,uint16_t source){mode_=SystemMode::Alarm;outputs_.siren=true;events_.append(now,Severity::Alarm,source,"alarm entered");}
ZoneState Controller::update_zone(size_t i,bool closed,bool tamper,uint64_t now){if(i>=zones_.size())invalid_index("zone");const auto old=zone_states_[i];const auto s=zones_[i].update(closed,tamper,now);zone_states_[i]=s;if(s!=old)events_.append(now,s==ZoneState::Normal?Severity::Info:Severity::Warning,100+static_cast<uint16_t>(i),"zone changed");if((mode_==SystemMode::ArmedHome||mode_==SystemMode::ArmedAway)&&(s==ZoneState::Open||s==ZoneState::Tamper))enter_alarm(now,200+static_cast<uint16_t>(i));return s;}
PressureState Controller::update_pressure(size_t i,float v,uint64_t now){if(i>=pressures_.size())invalid_index("pressure");const auto old=pressure_states_[i];const auto s=pressures_[i].update(v);pressure_states_[i]=s;if(s!=old)events_.append(now,s==PressureState::Normal?Severity::Info:Severity::Warning,300+static_cast<uint16_t>(i),"pressure changed");return s;}
Transport Controller::update_links(LinkInputs links,uint64_t now){const auto old=network_.active();const auto t=network_.update(links,now);if(t!=old)events_.append(now,Severity::Info,400,"transport changed");return t;}
Challenge Controller::issue_challenge(CommandType t,uint64_t now,uint32_t ttl){return challenges_.issue(t,now,ttl);}
CommandResult Controller::execute(const Command& c,uint64_t now){if(!c.authenticated)return {CommandCode::Unauthorized,false,false};if(!idempotency_.accept(c.request_id,c.issued_at_ms,now,config_.request_ttl_ms))return {CommandCode::Duplicate,false,true};if(dangerous(c.type)&&!challenges_.consume(c.type,c.challenge,now))return {CommandCode::ChallengeInvalid,false,false};switch(c.type){case CommandType::ArmHome:mode_=SystemMode::ArmedHome;break;case CommandType::ArmAway:mode_=SystemMode::ArmedAway;break;case CommandType::Disarm:mode_=SystemMode::Disarmed;outputs_.siren=false;break;case CommandType::Silence:outputs_.siren=false;break;case CommandType::OpenValves:if(mode_==SystemMode::Alarm||maintenance_.active())return {CommandCode::Unsafe,false,false};outputs_.valve1=outputs_.valve2=true;break;case CommandType::CloseValves:outputs_.valve1=outputs_.valve2=false;break;case CommandType::ResetAlarm:mode_=SystemMode::Disarmed;outputs_.siren=false;break;case CommandType::EnterMaintenance:maintenance_.enter(now);mode_=SystemMode::Maintenance;outputs_={};break;case CommandType::ExitMaintenance:maintenance_.exit();mode_=SystemMode::Disarmed;break;}events_.append(now,Severity::Info,500,"command executed");return {CommandCode::Accepted,true,false};}
TelemetryFrame Controller::telemetry(uint64_t up,uint64_t rtc){return telemetry_.build(up,rtc,mode_,network_.active(),zone_states_,pressure_states_,health_);}
}
