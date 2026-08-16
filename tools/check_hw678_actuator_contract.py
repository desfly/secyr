#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware/esp-idf/main"
INC = ROOT / "firmware/include/homeguard"
SRC = ROOT / "firmware/src"
TESTS = ROOT / "tests"


def read(path: Path) -> str:
    if not path.is_file():
        raise SystemExit(f"missing contract file: {path.relative_to(ROOT)}")
    return path.read_text(encoding="utf-8")


cmake = read(MAIN / "CMakeLists.txt")
app = read(MAIN / "app_main.cpp")
mcp = read(MAIN / "hg_mcp23017.cpp")
mcp_backend = read(MAIN / "hg_mcp23017_output_backend.cpp")
supervisor = read(MAIN / "hg_output_supervisor.cpp")
service = read(MAIN / "hg_service_http.cpp")
commissioning_nvs = read(MAIN / "hg_commissioning_nvs.cpp")
hardware_verification = read(INC / "hardware_verification.hpp")
commissioning_state_h = read(INC / "commissioning_state.hpp")
commissioning_state_cpp = read(SRC / "commissioning_state.cpp")
hardware_profile = read(SRC / "hardware_profile.cpp")
physical_h = read(INC / "physical_output_runtime.hpp")
physical_cpp = read(SRC / "physical_output_runtime.cpp")
system_model = read(INC / "system_model.hpp")
boot = read(SRC / "boot_readiness.cpp")
output_http = read(MAIN / "hg_output_http.cpp")
t47 = read(TESTS / "test_build0047.cpp")
t49 = read(TESTS / "test_build0049.cpp")
t54 = read(TESTS / "test_build0054.cpp")

configure_off_pos = physical_cpp.find("for (const auto channel : kAllChannels)")
verification_assign_pos = physical_cpp.find("hardware_verified_ = hardware_verification_allows_outputs(hardware_)")
verification_block_pos = physical_cpp.find("if (!hardware_verified_)", verification_assign_pos)

checks = {
    "MCP backend compiled": '"hg_mcp23017_output_backend.cpp"' in cmake,
    "output supervisor compiled": '"hg_output_supervisor.cpp"' in cmake,
    "legacy GPIO backend excluded": '"hg_gpio_output_backend.cpp"' not in cmake,
    "legacy DeviceCommandRouter excluded": '"../../src/device_command_router.cpp"' not in cmake,
    "legacy DeviceApiModel excluded": '"../../src/device_api_model.cpp"' not in cmake,
    "legacy Controller excluded": '"../../src/controller.cpp"' not in cmake,
    "legacy LocalApi excluded": '"../../src/local_api.cpp"' not in cmake,
    "app uses MCP backend": "Mcp23017OutputBackend g_mcp_outputs" in app,
    "app starts output supervisor": "g_output_supervisor.start(" in app,
    "app has no legacy GPIO runtime": "GpioOutputBackend g_gpio_outputs" not in app,
    "MCP latch OFF before output direction": mcp.find("write_register(kOlatA, 0x00)") >= 0 and mcp.find("write_register(kOlatA, 0x00)") < mcp.find("write_register(kIodirA, 0x00)"),
    "MCP output write fallback OFF": "force_safe_outputs()" in mcp_backend,
    "MCP backend reads Port B": "read_inputs(std::uint8_t* value)" in mcp_backend,
    "MCP backend cold interlock": "kColdOpen = 2" in mcp_backend and "kColdClose = 3" in mcp_backend,
    "MCP backend hot interlock": "kHotOpen = 4" in mcp_backend and "kHotClose = 5" in mcp_backend,
    "hardware schema v2": "kSchemaVersion = 2" in hardware_verification,
    "commissioning schema v2": "kSchemaVersion = 2" in commissioning_state_h,
    "hardware NVS v2": 'kHardwareKey = "hardware_v2"' in commissioning_nvs,
    "commissioning NVS v2": 'kCommissioningKey = "state_v2"' in commissioning_nvs,
    "legacy hardware key isolated": 'kLegacyHardwareKey = "hardware_v1"' in commissioning_nvs,
    "legacy commissioning key isolated": 'kLegacyCommissioningKey = "state_v1"' in commissioning_nvs,
    "ESP32-S3 GPIO hole enforced": "gpio >= 0 && gpio <= 21" in hardware_profile and "gpio >= 26 && gpio <= 48" in hardware_profile,
    "legacy direct output pins forbidden": "legacy_direct_outputs_unassigned" in hardware_profile,
    "fixed HW678 I2C": "pins.i2c_sda == 4 && pins.i2c_scl == 5" in hardware_profile,
    "fixed HW678 W5500": "pins.w5500_mosi == 11" in hardware_profile and "pins.w5500_cs == 10" in hardware_profile,
    "command revision exists": "command_revision" in system_model,
    "backend supervised inputs required": "virtual bool read_inputs" in physical_h,
    "limit channels defined": "ColdValveOpenLimit = 0" in physical_h and "HotValveClosedLimit = 3" in physical_h,
    "motion direction state": "ValveMotionDirection" in physical_h,
    "safety fault latch": "safety_fault_latched" in physical_h and "latch_fault_locked" in physical_cpp,
    "target limit stop": "target_reached" in physical_cpp and "stop_valve_locked" in physical_cpp,
    "measured timeout stop": "configured_timeout_ms" in physical_cpp and "ValveTimeout" in physical_cpp,
    "contradictory limits fail closed": "limits.cold_open && limits.cold_closed" in physical_cpp and "ValveSafetyFault" in physical_cpp,
    "no-command valve STOP": "output == nullptr || !output->commanded" in physical_cpp,
    "runtime mutex serialized": "std::mutex" in physical_h and "std::scoped_lock" in physical_cpp,
    "20 ms independent supervisor": "pdMS_TO_TICKS(20)" in supervisor,
    "supervisor uses monotonic time": "esp_timer_get_time()" in supervisor,
    "supervisor owns physical synchronize": "runtime_->synchronize" in supervisor,
    "HTTP does not touch physical synchronize": "physical_->synchronize" not in output_http,
    "HTTP rejects unhealthy physical runtime": "physical_output_not_ready" in output_http,
    "valve limit polarity must be verified": "valve_limit_polarity_verified" in commissioning_state_h and "ValveSafetyUnverified" in commissioning_state_cpp,
    "cold measured timeout required": "cold_valve_travel_timeout_ms == 0U" in commissioning_state_cpp,
    "hot measured timeout required": "hot_valve_travel_timeout_ms == 0U" in commissioning_state_cpp,
    "actuator test mandatory": "successful_actuator_tests > 0U" in commissioning_state_cpp,
    "boot exposes valve profile gate": "BlockedValveSafetyProfileRequired" in boot and "valve_safety_profile_required" in boot,
    "boot exposes actuator test gate": "BlockedActuatorTestRequired" in boot and "actuator_test_required" in boot,
    "destructive service uses sticky lockout": service.count("lockout_fail_closed()") >= 2,
    "destructive service rejects unsafe lockout": "output_safe_failed" in service,
    "OFF channels configured before verification": configure_off_pos >= 0 and verification_assign_pos > configure_off_pos and verification_block_pos > verification_assign_pos,
    "invalid hardware keeps supervisor-capable OFF runtime": verification_block_pos >= 0 and "return true;" in physical_cpp[verification_block_pos:verification_block_pos + 260],
    "bench requires hardware and dry-run": "hardware_verified_ &&" in physical_cpp and "commissioning_.successful_dry_runs > 0U" in physical_cpp,
    "bench valve profile mandatory": "commissioning_.valve_limit_polarity_verified" in physical_cpp and "valve_bench_channel(channel)" in physical_cpp,
    "bench pre-reads end stops": "LimitSnapshot before{}" in physical_cpp and "read_limits_locked(before)" in physical_cpp,
    "bench does not drive active target": "target_limit_active(" in physical_cpp and "before.cold_open" in physical_cpp,
    "bench post-reads end stops": "LimitSnapshot after{}" in physical_cpp and "read_limits_locked(after)" in physical_cpp,
    "bench success is end-stop evidence": "target_limit_reached = target_limit_active(" in physical_cpp and "return target_limit_reached" in physical_cpp,
    "maintenance accepts SystemModel snapshot": "set_maintenance_mode(bool active, const SystemModel& model)" in physical_h,
    "maintenance consumes all actuator revisions": "consume_current_revisions" in physical_cpp and "siren_command_revision_ = siren.command_revision" in physical_cpp and "state_.cold_valve.command_revision = cold_valve.command_revision" in physical_cpp and "state_.hot_valve.command_revision = hot_valve.command_revision" in physical_cpp,
    "service wires SystemModel into maintenance": "set_maintenance_mode(active, *self->model_)" in service,
    "commissioning test blocks missing valve safety": "ValveSafetyUnverified" in t47,
    "readiness test blocks missing valve profile": "BlockedValveSafetyProfileRequired" in t49,
    "physical test checks limit stop": "ColdValveOpenLimit" in t54 and "limit_stops" in t54,
    "physical test checks timeout latch": "ValveTimeout" in t54 and "safety_fault_latched" in t54,
    "physical test checks contradictory limits": "ColdValveClosedLimit" in t54 and "ValveSafetyFault" in t54,
    "physical test checks read failure": "fail_reads" in t54 and "BackendError" in t54,
    "physical test checks uncommissioned OFF path": "missing_hardware" in t54 and "lockout_fail_closed()" in t54,
    "physical test checks end-stop transition": "bench_transition_cold_open = true" in t54 and "bench_delay_seen == 0U" in t54,
    "physical test checks pre-maintenance pending command": "model.set_output_active(2, true, 101)" in t54 and "runtime.synchronize(model, 102)" in t54,
    "physical test checks in-maintenance pending command": "model.set_output_active(1, false, 104)" in t54 and "runtime.synchronize(model, 105)" in t54,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"{'PASS' if ok else 'FAIL'}: {name}")

if failed:
    raise SystemExit("HW-678 actuator contract failed: " + ", ".join(failed))

print(f"HW-678 actuator contract PASS ({len(checks)} checks)")
