#pragma once
#include <cstddef>
#include "homeguard/challenge.hpp"
#include "homeguard/command.hpp"
#include "homeguard/config.hpp"
#include "homeguard/event_log.hpp"
#include "homeguard/health_monitor.hpp"
#include "homeguard/idempotency.hpp"
#include "homeguard/maintenance.hpp"
#include "homeguard/network_failover.hpp"
#include "homeguard/pressure.hpp"
#include "homeguard/telemetry.hpp"
#include "homeguard/zone.hpp"
#include <array>
namespace hg {
class Controller {
public:
 explicit Controller(ControllerConfig config={});
 ZoneState update_zone(size_t index, bool loop_closed, bool tamper, uint64_t now_ms);
 PressureState update_pressure(size_t index, float value, uint64_t now_ms);
 Transport update_links(LinkInputs links, uint64_t now_ms);
 CommandResult execute(const Command& command, uint64_t now_ms);
 Challenge issue_challenge(CommandType type, uint64_t now_ms, uint32_t ttl_ms=30000);
 TelemetryFrame telemetry(uint64_t uptime_ms, uint64_t rtc_epoch);
 [[nodiscard]] SystemMode mode() const { return mode_; }
 [[nodiscard]] Outputs outputs() const { return maintenance_.apply(outputs_); }
 [[nodiscard]] const EventLog& events() const { return events_; }
 [[nodiscard]] HealthMonitor& health() { return health_; }
 [[nodiscard]] const HealthMonitor& health() const { return health_; }
 [[nodiscard]] Transport transport() const { return network_.active(); }
private:
 void enter_alarm(uint64_t now_ms, uint16_t source);
 ControllerConfig config_{}; std::array<ZoneEvaluator,5> zones_{}; std::array<PressureEvaluator,2> pressures_{}; std::array<ZoneState,5> zone_states_{}; std::array<PressureState,2> pressure_states_{};
 EventLog events_{}; ChallengeManager challenges_{}; IdempotencyCache idempotency_{}; HealthMonitor health_{}; NetworkFailover network_; TelemetryBuilder telemetry_{}; MaintenanceGuard maintenance_{}; SystemMode mode_{SystemMode::Disarmed}; Outputs outputs_{};
};
}
