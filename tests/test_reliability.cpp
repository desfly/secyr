#include "test_framework.hpp"
#include "homeguard/boot_self_test.hpp"
#include "homeguard/challenge.hpp"
#include "homeguard/crc32.hpp"
#include "homeguard/event_log.hpp"
#include "homeguard/idempotency.hpp"
#include "homeguard/maintenance.hpp"
#include "homeguard/network_failover.hpp"
#include "homeguard/telemetry.hpp"
#include <array>
void test_reliability(){
 hg::IdempotencyCache cache; CHECK(cache.accept(1,100,100,1000)); CHECK(!cache.accept(1,100,200,1000)); CHECK(!cache.accept(2,100,1200,1000)); CHECK(!cache.accept(0,100,100,1000)); CHECK(!cache.accept(3,200,100,1000));
 hg::ChallengeManager cm; auto ch=cm.issue(hg::CommandType::OpenValves,100,50); CHECK(ch.valid); CHECK(!cm.consume(hg::CommandType::CloseValves,ch.token,120)); auto ch2=cm.issue(hg::CommandType::OpenValves,100,50); CHECK(cm.consume(hg::CommandType::OpenValves,ch2.token,150)); CHECK(!cm.consume(hg::CommandType::OpenValves,ch2.token,150)); auto ch3=cm.issue(hg::CommandType::ResetAlarm,100,10); CHECK(!cm.consume(hg::CommandType::ResetAlarm,ch3.token,111));
 hg::HealthMonitor h; CHECK(h.overall()==hg::HealthState::Unknown); h.report(hg::Component::Rtc,false,1); CHECK(h.get(hg::Component::Rtc).state==hg::HealthState::Degraded); h.report(hg::Component::Rtc,false,2); h.report(hg::Component::Rtc,false,3); CHECK(h.get(hg::Component::Rtc).state==hg::HealthState::Failed); CHECK(h.failed_count()==1); h.report(hg::Component::Rtc,true,4); CHECK(h.get(hg::Component::Rtc).state==hg::HealthState::Ok);
 hg::NetworkFailover net(100,500); CHECK(net.update({true,true,true},0)==hg::Transport::None); CHECK(net.update({true,true,true},100)==hg::Transport::Ethernet); CHECK(net.update({false,true,true},150)==hg::Transport::Ethernet); CHECK(net.update({false,true,true},250)==hg::Transport::WifiSta); CHECK(net.update({true,true,true},300)==hg::Transport::WifiSta); CHECK(net.update({true,true,true},800)==hg::Transport::Ethernet); CHECK(net.update({true,true,true},900)==hg::Transport::Ethernet);
 hg::MaintenanceGuard mg; hg::Outputs o{true,true,true,true,true}; CHECK(mg.apply(o).siren); mg.enter(55); CHECK(mg.active()); CHECK(mg.entered_at()==55); auto safe=mg.apply(o); CHECK(!safe.siren&&!safe.valve1&&!safe.valve2&&!safe.aux1&&!safe.aux2); mg.exit(); CHECK(!mg.active());
 hg::EventLog log; for(int i=0;i<130;++i)log.append(i,hg::Severity::Info,static_cast<uint16_t>(i),"x"); CHECK(log.size()==128); CHECK(log.at_oldest(0).sequence==3); CHECK(log.at_oldest(127).sequence==130);
 std::array<std::byte,3> bytes{std::byte{1},std::byte{2},std::byte{3}}; CHECK(hg::crc32(bytes)==0x55BC801DU);
 hg::HealthMonitor hs; auto overall=hg::BootSelfTest::run({true,true,true,true,true,true,true,true},hs,0); CHECK(overall==hg::HealthState::Ok); CHECK(hs.failed_count()==0);
}
