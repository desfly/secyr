from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
errors: list[str] = []


def text(path: str) -> str:
    target = ROOT / path
    if not target.is_file():
        errors.append(f"missing concurrency source: {path}")
        return ""
    return target.read_text(encoding="utf-8")


def require(body: str, token: str, label: str) -> None:
    if token not in body:
        errors.append(f"runtime concurrency contract regressed: {label}")


header = text("firmware/include/homeguard/system_model.hpp")
model = text("firmware/src/system_model.cpp")
physical = text("firmware/src/physical_output_runtime.cpp")
system_api = text("firmware/src/system_api.cpp")
telemetry = text("firmware/esp-idf/main/hg_telemetry_runtime.cpp")
output_http_hpp = text("firmware/esp-idf/main/hg_output_http.hpp")
output_http = text("firmware/esp-idf/main/hg_output_http.cpp")
service_http_hpp = text("firmware/esp-idf/main/hg_service_http.hpp")
service_http = text("firmware/esp-idf/main/hg_service_http.cpp")
system_http_hpp = text("firmware/esp-idf/main/hg_system_http.hpp")
system_http = text("firmware/esp-idf/main/hg_system_http.cpp")
app_main = text("firmware/esp-idf/main/app_main.cpp")

for token, label in [
    ("#include <mutex>", "SystemModel mutex header"),
    ("mutable std::mutex mutex_", "SystemModel storage mutex"),
    ("std::mutex dispatch_mutex_", "event callback dispatch mutex"),
    ("output_snapshot", "output snapshot API"),
    ("zone_at_snapshot", "zone index snapshot API"),
    ("partition_at_snapshot", "partition index snapshot API"),
]:
    require(header, token, label)

for token, label in [
    ("std::scoped_lock dispatch_lock(dispatch_mutex_)", "serialized subscriber callbacks"),
    ("std::scoped_lock lock(mutex_)", "model/event storage locking"),
    ("out = outputs_[i]", "output copy snapshot"),
    ("out = zones_[index]", "zone copy snapshot"),
    ("out = partitions_[index]", "partition copy snapshot"),
]:
    require(model, token, label)

for body, token, label in [
    (physical, "model.output_snapshot(1, siren)", "actuator supervisor siren snapshot"),
    (physical, "model.output_snapshot(2, cold_valve)", "actuator supervisor cold valve snapshot"),
    (physical, "model.output_snapshot(3, hot_valve)", "actuator supervisor hot valve snapshot"),
    (system_api, "model.zone_at_snapshot", "system API zone snapshots"),
    (system_api, "model.output_at_snapshot", "system API output snapshots"),
    (system_api, "model.partition_at_snapshot", "system API partition snapshots"),
    (telemetry, "partition_at_snapshot", "telemetry partition snapshot"),
    (telemetry, "zone_at_snapshot", "telemetry zone snapshot"),
    (output_http, "output_snapshot(output_id, output)", "HTTP output metadata snapshot"),
    (system_http_hpp, "mutable std::mutex event_mutex_", "SystemHttp event/client mutex"),
    (system_http, "std::scoped_lock lock(event_mutex_)", "SystemHttp event/client locking"),
    (system_http, "clients = clients_", "WebSocket client copy-under-lock"),
]:
    require(body, token, label)

# Mutable hardware verification / commissioning / boot readiness are shared by
# output and destructive-service HTTP paths. They must use one common lock.
for body, token, label in [
    (app_main, "std::mutex g_control_state_mutex", "global control-state mutex"),
    (output_http_hpp, "std::mutex* control_state_mutex_", "output HTTP control-state mutex member"),
    (service_http_hpp, "std::mutex* control_state_mutex_", "service HTTP control-state mutex member"),
    (output_http, "std::scoped_lock lock(*control_state_mutex_)", "output readiness snapshot lock"),
    (output_http, "readiness_snapshot = *readiness_", "output readiness copied under lock"),
    (service_http, "std::scoped_lock lock(*self->control_state_mutex_)", "service shared-state lock"),
]:
    require(body, token, label)

if app_main.count("&g_control_state_mutex") < 2:
    errors.append("runtime concurrency contract regressed: shared control mutex not wired to both HTTP modules")
if service_http.count("std::scoped_lock lock(*self->control_state_mutex_)") < 3:
    errors.append("runtime concurrency contract regressed: service readiness/invalidate/reset are not all serialized")

# Runtime SystemModel readers must use copy snapshots. Avoid broad '.output('
# matching because unrelated classes may legitimately expose output() methods.
forbidden = (
    "model.output(", "model_->output(", "system_model_->output(",
    "model.zone_at(", "model_->zone_at(", "system_model_->zone_at(",
    "model.sensor_at(", "model_->sensor_at(", "system_model_->sensor_at(",
    "model.output_at(", "model_->output_at(", "system_model_->output_at(",
    "model.partition_at(", "model_->partition_at(", "system_model_->partition_at(",
)

scan_roots = [ROOT / "firmware" / "src", ROOT / "firmware" / "esp-idf" / "main"]
for scan_root in scan_roots:
    for path in scan_root.glob("*.cpp"):
        if path.name == "system_model.cpp":
            continue
        body = path.read_text(encoding="utf-8")
        for token in forbidden:
            if token in body:
                errors.append(
                    f"runtime concurrency contract regressed: raw SystemModel view {token} in {path.relative_to(ROOT)}"
                )

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Runtime concurrency contract: PASS")
