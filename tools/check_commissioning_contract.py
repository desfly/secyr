#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
errors: list[str] = []


def read(path: str) -> str:
    target = ROOT / path
    if not target.is_file():
        errors.append(f"missing commissioning source: {path}")
        return ""
    return target.read_text(encoding="utf-8")


def require(body: str, token: str, label: str) -> None:
    if token not in body:
        errors.append(f"commissioning contract regressed: {label}")


service_h = read("firmware/esp-idf/main/hg_service_http.hpp")
service = read("firmware/esp-idf/main/hg_service_http.cpp")
nvs = read("firmware/esp-idf/main/hg_commissioning_nvs.cpp")
state_h = read("firmware/include/homeguard/commissioning_state.hpp")
state_cpp = read("firmware/src/commissioning_state.cpp")
readiness = read("firmware/src/boot_readiness.cpp")
physical_h = read("firmware/include/homeguard/physical_output_runtime.hpp")
physical = read("firmware/src/physical_output_runtime.cpp")
supervisor_h = read("firmware/esp-idf/main/hg_output_supervisor.hpp")
supervisor = read("firmware/esp-idf/main/hg_output_supervisor.cpp")
app = read("firmware/esp-idf/main/app_main.cpp")
t47 = read("tests/test_build0047.cpp")
t49 = read("tests/test_build0049.cpp")
t54 = read("tests/test_build0054.cpp")

routes = [
    "/api/v1/service/maintenance",
    "/api/v1/service/commissioning/hardware-verify",
    "/api/v1/service/commissioning/dry-run",
    "/api/v1/service/commissioning/valve-profile",
    "/api/v1/service/commissioning/bench-pulse",
    "/api/v1/service/commissioning/actuator-accept",
]
for route in routes:
    require(service, route, f"live route {route}")

for command in [
    "system.commissioning.maintenance",
    "system.commissioning.hardware_verify",
    "system.commissioning.dry_run",
    "system.commissioning.valve_profile",
    "system.commissioning.bench_pulse",
    "system.commissioning.actuator_accept",
]:
    require(service, command, f"admin-only audit command {command}")

for token, label in [
    ('confirmation != "HW678_MCP23017_VERIFIED"', "explicit hardware signoff"),
    ('confirmation != "DRY_RUN_PASS"', "explicit dry-run signoff"),
    ('confirmation != "VALVE_PROFILE_VERIFIED"', "explicit valve profile signoff"),
    ('confirmation != "BENCH_PULSE"', "explicit bench pulse signoff"),
    ('confirmation != "ACTUATOR_TEST_PASS"', "explicit actuator signoff"),
    ("maintenance_required", "maintenance gate"),
    ("PartitionArmState::Disarmed", "bench pulse disarmed gate"),
    ("live_mcp_ready", "physical MCP presence gate"),
    ("dry_run_hardware_ready", "dry-run hardware readiness"),
    ("bench_valve_mask_ != 0x0FU", "all four valve directions mandatory"),
    ("bench_valve_mask_ |= valve_mask", "bench direction evidence accumulation"),
    ("coldTimeoutMs", "measured cold timeout input"),
    ("hotTimeoutMs", "measured hot timeout input"),
    ("activeLow", "measured limit polarity input"),
    ("refresh_control_state_from_store", "dynamic readiness refresh"),
]:
    require(service, token, label)

# Optional modules may make bootstrap Degraded but cannot deadlock the actuator
# commissioning sequence. Only the I2C/MCP fail-closed core is mandatory here.
dry_start = service.find("bool dry_run_hardware_ready")
dry_end = service.find("bool target_channel", dry_start)
if dry_start < 0 or dry_end < 0:
    errors.append("commissioning contract regressed: dry-run readiness helper missing")
else:
    dry_helper = service[dry_start:dry_end]
    require(dry_helper, "return live_mcp_ready(hardware);", "dry-run requires only actuator safety core")
    for forbidden in ["ads1115", "ina226", "ds3231", "storage", "ethernet", "w5500"]:
        if forbidden in dry_helper.lower():
            errors.append(f"commissioning contract regressed: optional module '{forbidden}' blocks dry-run")

require(state_h, "commissioning_state_persistable", "partial commissioning persistence API")
require(state_cpp, "bool commissioning_state_persistable", "partial commissioning persistence implementation")
require(nvs, "commissioning_state_persistable(state)", "NVS accepts safe partial progress")
if "validate_commissioning_state(state) == hg::CommissioningStateValidation::Valid" in nvs:
    errors.append("commissioning contract regressed: NVS again requires fully Valid state for all persistence")

for token, label in [
    ("kMaxBenchPulseMs = 1000U", "hard one-second bench ceiling"),
    ("bool set_maintenance_mode(bool active)", "actuator maintenance gate"),
    ("bool update_control_state", "actuator-local control state refresh"),
    ("bool bench_pulse", "bounded commissioning pulse API"),
    ("bool maintenance_mode{}", "maintenance state visibility"),
]:
    require(physical_h, token, label)

for token, label in [
    ("!state_.maintenance_mode", "bench pulse requires actuator maintenance mode"),
    ("duration_ms > kMaxBenchPulseMs", "bench pulse maximum enforced"),
    ("force_safe_locked()", "bench/off fail-safe transitions"),
    ("state_.maintenance_mode", "normal supervisor maintenance block"),
    ("siren.command_revision != siren_command_revision_", "siren stale-command suppression"),
]:
    require(physical, token, label)

if "BootReadinessReport*" in supervisor_h or "readiness_" in supervisor_h:
    errors.append("commissioning contract regressed: supervisor again depends on mutable global readiness")
require(supervisor, "runtime_->synchronize(*model_, now_ms)", "supervisor uses actuator-local readiness")
require(app, "g_output_supervisor.start(\n            &g_physical_outputs,\n            &g_system_model)", "two-argument actuator-local supervisor wiring")
require(app, "&g_system_bus, &g_system_model, &g_hardware, &g_control_state_mutex", "commissioning runtime wiring")

# Exact field-test order must remain visible and test-covered.
sequence = [
    "BlockedDryRunRequired",
    "BlockedValveSafetyProfileRequired",
    "BlockedActuatorTestRequired",
    "ReadyForPhysicalOutputs",
]
positions = [readiness.find(token) for token in sequence]
if any(pos < 0 for pos in positions) or positions != sorted(positions):
    errors.append("commissioning contract regressed: readiness order is not dry-run -> valve-profile -> actuator-test -> ready")
for token in ["dry_run_required", "valve_safety_profile_required", "actuator_test_required"]:
    require(t49, token, f"unit test covers {token}")
require(t47, "commissioning_state_persistable", "unit test covers partial persistence")
require(t54, "kMaxBenchPulseMs + 1U", "unit test rejects overlong bench pulse")
require(t54, "set_maintenance_mode(true)", "unit test covers maintenance bench gate")

# Destructive operations use sticky fail-closed. Factory reset always reboots;
# commissioning invalidation must reboot too, otherwise the sticky latch cannot
# be legally cleared by a same-boot recommissioning attempt.
if service.count("lockout_fail_closed()") < 2:
    errors.append("commissioning contract regressed: destructive routes are not sticky fail-closed")
require(service, "nvs_flash_erase()", "whole-NVS factory reset")
if service.count("esp_restart();") < 2:
    errors.append("commissioning contract regressed: invalidate and factory-reset do not both reboot after sticky lockout")
invalidate_start = service.find("esp_err_t ServiceHttp::invalidate_post")
invalidate_end = service.find("esp_err_t ServiceHttp::factory_reset_post", invalidate_start)
if invalidate_start < 0 or invalidate_end < 0:
    errors.append("commissioning contract regressed: invalidate route implementation missing")
else:
    invalidate_body = service[invalidate_start:invalidate_end]
    require(invalidate_body, '"state\\\":\\\"restarting"', "commissioning invalidation reports restart")
    require(invalidate_body, "esp_restart();", "commissioning invalidation reboots after sticky lockout")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Commissioning contract: PASS")
