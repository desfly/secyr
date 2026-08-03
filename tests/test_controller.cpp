#include "test_framework.hpp"
#include "homeguard/controller.hpp"
void test_controller(){
 hg::ControllerConfig cfg; cfg.network_debounce_ms=0; cfg.failover_hold_ms=0; cfg.request_ttl_ms=1000; for(auto&z:cfg.zones)z.debounce_ms=0;
 hg::Controller c(cfg); CHECK(c.mode()==hg::SystemMode::Disarmed); CHECK(c.events().size()==1);
 CHECK(c.update_links({true,false,true},0)==hg::Transport::Ethernet);
 hg::Command arm{1,10,hg::CommandType::ArmAway,0,true}; auto r=c.execute(arm,10); CHECK(r.executed); CHECK(c.mode()==hg::SystemMode::ArmedAway);
 CHECK(c.update_zone(0,false,false,20)==hg::ZoneState::Open); CHECK(c.mode()==hg::SystemMode::Alarm); CHECK(c.outputs().siren);
 hg::Command unauth{2,30,hg::CommandType::Disarm,0,false}; CHECK(c.execute(unauth,30).code==hg::CommandCode::Unauthorized);
 hg::Command disarm{3,30,hg::CommandType::Disarm,0,true}; CHECK(c.execute(disarm,30).executed); CHECK(c.mode()==hg::SystemMode::Disarmed); CHECK(!c.outputs().siren); CHECK(c.execute(disarm,31).duplicate);
 auto ch=c.issue_challenge(hg::CommandType::OpenValves,40); hg::Command open{4,40,hg::CommandType::OpenValves,ch.token,true}; CHECK(c.execute(open,40).executed); CHECK(c.outputs().valve1&&c.outputs().valve2);
 hg::Command maint{5,50,hg::CommandType::EnterMaintenance,0,true}; CHECK(c.execute(maint,50).executed); CHECK(c.mode()==hg::SystemMode::Maintenance); CHECK(!c.outputs().valve1);
 auto exit_ch=c.issue_challenge(hg::CommandType::ExitMaintenance,60); hg::Command exit{6,60,hg::CommandType::ExitMaintenance,exit_ch.token,true}; CHECK(c.execute(exit,60).executed); CHECK(c.mode()==hg::SystemMode::Disarmed);
 c.health().set(hg::Component::Esp,hg::HealthState::Ok,0); auto t1=c.telemetry(100,1700000000); auto t2=c.telemetry(101,1700000001); CHECK(t1.sequence==1); CHECK(t2.sequence==2); CHECK(t1.crc!=0); CHECK(t1.transport==hg::Transport::Ethernet);
 CHECK(c.update_pressure(0,0.5F,100)==hg::PressureState::Low); CHECK(c.events().size()>=8);
}
