#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
errors: list[str] = []


def read(path: str) -> str:
    target = ROOT / path
    if not target.is_file():
        errors.append(f"missing diagnostics source: {path}")
        return ""
    return target.read_text(encoding="utf-8")


def require(body: str, token: str, label: str) -> None:
    if token not in body:
        errors.append(f"physical diagnostics contract regressed: {label}")


header = read("firmware/include/homeguard/physical_output_runtime.hpp")
runtime = read("firmware/src/physical_output_runtime.cpp")
service = read("firmware/esp-idf/main/hg_service_http.cpp")
test = read("tests/test_build0059.cpp")
main = read("tests/test_main.cpp")
declarations = read("tests/test_declarations.hpp")
cmake = read("firmware/CMakeLists.txt")

for token, label in [
    ("bool limit_inputs_valid{}", "limit-input validity field"),
    ("std::uint8_t raw_limit_inputs{}", "raw GPB input field"),
    ("bool cold_open_limit{}", "cold-open decoded field"),
    ("bool cold_closed_limit{}", "cold-closed decoded field"),
    ("bool hot_open_limit{}", "hot-open decoded field"),
    ("bool hot_closed_limit{}", "hot-closed decoded field"),
]:
    require(header, token, label)

for token, label in [
    ("state_.limit_inputs_valid = false", "failed read invalidates diagnostics"),
    ("state_.raw_limit_inputs = raw", "raw GPB snapshot assignment"),
    ("state_.cold_open_limit = limits.cold_open", "cold-open snapshot assignment"),
    ("state_.cold_closed_limit = limits.cold_closed", "cold-closed snapshot assignment"),
    ("state_.hot_open_limit = limits.hot_open", "hot-open snapshot assignment"),
    ("state_.hot_closed_limit = limits.hot_closed", "hot-closed snapshot assignment"),
    ("state_.limit_inputs_valid = true", "successful read marks diagnostics valid"),
]:
    require(runtime, token, label)

for token, label in [
    ('"limitInputsValid"', "API limit validity"),
    ('"rawLimitInputs"', "API raw GPB byte"),
    ('"coldOpenLimit"', "API cold-open limit"),
    ('"coldClosedLimit"', "API cold-closed limit"),
    ('"hotOpenLimit"', "API hot-open limit"),
    ('"hotClosedLimit"', "API hot-closed limit"),
    ('"coldValveMotion"', "API cold valve motion"),
    ('"hotValveMotion"', "API hot valve motion"),
    ('"coldValveTimeoutMs"', "API cold valve timeout"),
    ('"hotValveTimeoutMs"', "API hot valve timeout"),
    ('"physicalSafetyFaultLatched"', "API sticky fault state"),
    ('"physicalFailures"', "API physical failure counter"),
    ('"limitStops"', "API limit-stop counter"),
    ('"valveTimeouts"', "API valve-timeout counter"),
]:
    require(service, token, label)

# HTTP diagnostics must be read-only with respect to the physical bus. The
# readiness endpoint is allowed to take a runtime snapshot, not poll MCP/I2C.
readiness_start = service.find("esp_err_t ServiceHttp::readiness_get")
readiness_end = service.find("esp_err_t ServiceHttp::maintenance_post", readiness_start)
if readiness_start < 0 or readiness_end < 0:
    errors.append("physical diagnostics contract regressed: readiness handler missing")
else:
    readiness = service[readiness_start:readiness_end]
    require(readiness, "physical_outputs_->state()", "readiness uses mutex-protected physical snapshot")
    for forbidden in ("read_inputs(", "i2c_", "io_expander", "read_limits_locked"):
        if forbidden in readiness:
            errors.append(f"physical diagnostics contract regressed: readiness performs live bus access via {forbidden}")

for token, label in [
    ("state.limit_inputs_valid", "test validates successful GPB snapshot"),
    ("state.raw_limit_inputs == 0xFFU", "test validates idle raw GPB"),
    ("state.raw_limit_inputs == 0xFEU", "test validates cold-open raw GPB"),
    ("state.raw_limit_inputs == 0xFCU", "test preserves contradictory raw GPB"),
    ("state.cold_open_limit", "test validates decoded cold-open"),
    ("state.cold_closed_limit", "test validates decoded cold-closed"),
    ("!read_failure_state.limit_inputs_valid", "test distinguishes read failure from inactive limit"),
    ("PhysicalOutputStatus::ValveSafetyFault", "test preserves contradictory limit fault"),
    ("PhysicalOutputStatus::BackendError", "test covers GPB read failure"),
]:
    require(test, token, label)

require(declarations, "void test_build0059();", "Build-0059 declaration")
require(main, "test_build0059();", "Build-0059 execution")
require(cmake, "../tests/test_build0059.cpp", "Build-0059 linked into host test binary")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Physical diagnostics contract: PASS")
