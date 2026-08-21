from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "firmware" / "esp-idf" / "main" / "hg_reset_sequence.cpp"
HEADER = ROOT / "firmware" / "include" / "homeguard" / "reset_sequence.hpp"

runtime = RUNTIME.read_text(encoding="utf-8")
header = HEADER.read_text(encoding="utf-8")
errors = []

# Normative timing/count contract agreed for the service-button reset gesture.
for expected in (
    "kFactoryResetRequiredHolds = 3U",
    "kFactoryResetHoldMs = 1500U",
    "kFactoryResetSequenceTimeoutMs = 5000U",
    "kFactoryResetSuccessWhiteMs = 5000U",
):
    if expected not in header:
        errors.append(f"reset contract changed: missing {expected}")

# Runtime constants must still match the physical contract.
for expected in (
    "constexpr std::uint8_t kRequiredHolds = 3U;",
    "constexpr TickType_t kHoldTicks = pdMS_TO_TICKS(1500);",
    "constexpr TickType_t kSequenceTimeoutTicks = pdMS_TO_TICKS(5000);",
    "constexpr TickType_t kSuccessWhiteTicks = pdMS_TO_TICKS(5000);",
):
    if expected not in runtime:
        errors.append(f"RST runtime timing/count drift: missing {expected}")

# RED must be the acknowledgement for a long-enough hold, and a failed RED
# output must never arm/count the hold.
red_call = runtime.find("RgbDiagnostic::set_red(board::kOnboardRgb)")
red_success = runtime.find("hold_confirmed = true;", red_call)
red_failure = runtime.find("Cannot show RED hold confirmation; hold not armed", red_call)
if min(red_call, red_success, red_failure) < 0:
    errors.append("RED hold-confirmation path is incomplete")
elif red_failure > red_success:
    # In the source, the failure branch text should appear before the success
    # assignment in the following else branch.
    errors.append("RED failure must be handled before the hold is armed")

# Release after a RED-confirmed hold must turn RED off, advance exactly one step,
# and only the third confirmed release may stage destructive reset.
release_block = runtime.find("if (hold_confirmed) {")
release_off = runtime.find("RgbDiagnostic::off(board::kOnboardRgb)", release_block)
advance = runtime.find("hg::advance_confirmed_hold(confirmed_holds, true, kRequiredHolds)", release_block)
trigger = runtime.find("if (step.trigger_factory_reset) stage_factory_reset_and_reboot();", release_block)
if min(release_block, release_off, advance, trigger) < 0:
    errors.append("confirmed-release path is incomplete")
elif not (release_block < release_off < advance < trigger):
    errors.append("confirmed release must turn RED off, advance one hold, then gate Factory Reset")

# The destructive action must be staged safely for early boot before reboot.
stage_fn = runtime.find("void stage_factory_reset_and_reboot()")
stage_request = runtime.find("stage_factory_reset_request()", stage_fn)
stage_restart = runtime.find("esp_restart();", stage_request)
if min(stage_fn, stage_request, stage_restart) < 0 or not (stage_fn < stage_request < stage_restart):
    errors.append("Factory Reset must be staged before reboot")

# A successful early-boot erase is acknowledged by WHITE for exactly the
# contract duration, then RGB is turned off and the controller reboots cleanly.
white_call = runtime.find("RgbDiagnostic::set_white(board::kOnboardRgb)")
white_delay = runtime.find("vTaskDelay(kSuccessWhiteTicks);", white_call)
white_off = runtime.find("RgbDiagnostic::off(board::kOnboardRgb)", white_delay)
white_restart = runtime.find("esp_restart();", white_off)
if min(white_call, white_delay, white_off, white_restart) < 0:
    errors.append("WHITE success-confirmation path is incomplete")
elif not (white_call < white_delay < white_off < white_restart):
    errors.append("Factory Reset success must be WHITE -> 5 s delay -> OFF -> reboot")

# An unfinished gesture must expire instead of accidentally completing later.
if "hg::expire_reset_gesture(confirmed_holds, timed_out)" not in runtime:
    errors.append("RST sequence timeout guard missing")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("RST/RGB factory-reset contract PASS")
