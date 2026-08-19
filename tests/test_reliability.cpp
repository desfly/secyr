#include "test_framework.hpp"
#include "homeguard/boot_self_test.hpp"
#include "homeguard/crc32.hpp"
#include "homeguard/event_log.hpp"
#include "homeguard/health_monitor.hpp"

#include <array>

void test_reliability() {
    hg::HealthMonitor health;
    CHECK(health.overall() == hg::HealthState::Unknown);
    health.report(hg::Component::Rtc, false, 1);
    CHECK(health.get(hg::Component::Rtc).state == hg::HealthState::Degraded);
    health.report(hg::Component::Rtc, false, 2);
    health.report(hg::Component::Rtc, false, 3);
    CHECK(health.get(hg::Component::Rtc).state == hg::HealthState::Failed);
    CHECK(health.failed_count() == 1);
    health.report(hg::Component::Rtc, true, 4);
    CHECK(health.get(hg::Component::Rtc).state == hg::HealthState::Ok);

    hg::EventLog log;
    for (int i = 0; i < 130; ++i) {
        log.append(i, hg::Severity::Info, static_cast<std::uint16_t>(i), "x");
    }
    CHECK(log.size() == 128);
    CHECK(log.at_oldest(0).sequence == 3);
    CHECK(log.at_oldest(127).sequence == 130);

    std::array<std::byte, 3> bytes{std::byte{1}, std::byte{2}, std::byte{3}};
    CHECK(hg::crc32(bytes) == 0x55BC801DU);

    hg::HealthMonitor self_test_health;
    const auto overall = hg::BootSelfTest::run(
        {true, true, true, true, true, true, true, true},
        self_test_health,
        0);
    CHECK(overall == hg::HealthState::Ok);
    CHECK(self_test_health.failed_count() == 0);
}
