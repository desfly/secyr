from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "firmware" / "esp-idf" / "main" / "hg_reset_sequence.cpp"
HEADER = ROOT / "firmware" / "include" / "homeguard" / "reset_sequence.hpp"

runtime = RUNTIME.read_text(encoding="utf-8")
header = HEADER.read_text(encoding="utf-8")
errors = []

# Normative timing/count contract agreed for the reset gesture.
for expected in (
    "kFactoryResetRequiredHolds = 3U",
    "kFactoryResetHoldMs = 1500U",
    "kFactoryResetSequenceTimeoutMs = 5000U",
    "kFactoryResetSuccessRedMs = 5000U",
):
    if expected not in header:
        errors.append(f"reset contract changed: missing {expected}")

# Runtime constants must still match the approved physical contract.
for expected in (
    "constexpr std::uint8_t kRequiredHolds = 3U;",
    "constexpr TickType_t kHoldTicks = pdMS_TO_TICKS(1500);",
    "constexpr TickType_t kSequenceTimeoutTicks = pdMS_TO_TICKS(5000);",
    "constexpr TickType_t kSuccessRedTicks = pdMS_TO_TICKS(5000);",
):
    if expected not in runtime:
        errors.append(f"RST runtime timing/count drift: missing {expected}")

# WHITE is the acknowledgement for a long-enough hold. If WHITE cannot be
# shown, the hold must not be armed or counted.
white_call = runtime.find("RgbDiagnostic::set_white(board::kOnboardRgb)")
white_failure = runtime.find("Cannot show WHITE hold confirmation; hold not armed", white_call)
white_success = runtime.find("hold_confirmed = true;", white_call)
if min(white_call, white_failure, white_success) < 0:
    errors.append("WHITE hold-confirmation path is incomplete")
elif not (white_call < white_failure < white_success):
    errors.append("WHITE failure must be handled before the hold is armed")

# Release after a WHITE-confirmed hold must turn the LED off, advance exactly
# one step, and only the third confirmed release may stage Factory Reset.
release_block = runtime.find("if (hold_confirmed) {")
release_off = runtime.find("RgbDiagnostic::off(board::kOnboardRgb)", release_block)
advance = runtime.find("hg::advance_confirmed_hold(confirmed_holds, true, kRequiredHolds)", release_block)
trigger = runtime.find("if (step.trigger_factory_reset) stage_factory_reset_and_reboot();", release_block)
if min(release_block, release_off, advance, trigger) < 0:
    errors.append("confirmed-release path is incomplete")
elif not (release_block < release_off < advance < trigger):
    errors.append("confirmed release must turn WHITE off, advance one hold, then gate Factory Reset")

# The destructive action must be staged safely for early boot before reboot.
stage_fn = runtime.find("void stage_factory_reset_and_reboot()")
stage_request = runtime.find("stage_factory_reset_request()", stage_fn)
stage_restart = runtime.find("esp_restart();", stage_request)
if min(stage_fn, stage_request, stage_restart) < 0 or not (stage_fn < stage_request < stage_restart):
    errors.append("Factory Reset must be staged before reboot")

# A successful early-boot erase is acknowledged by RED for exactly five
# seconds, then RGB is turned off and the controller reboots cleanly.
red_call = runtime.find("RgbDiagnostic::set_red(board::kOnboardRgb)")
red_delay = runtime.find("vTaskDelay(kSuccessRedTicks);", red_call)
red_off = runtime.find("RgbDiagnostic::off(board::kOnboardRgb)", red_delay)
red_restart = runtime.find("esp_restart();", red_off)
if min(red_call, red_delay, red_off, red_restart) < 0:
    errors.append("RED success-confirmation path is incomplete")
elif not (red_call < red_delay < red_off < red_restart):
    errors.append("Factory Reset success must be RED -> 5 s delay -> OFF -> reboot")

# An unfinished gesture must expire instead of accidentally completing later.
if "hg::expire_reset_gesture(confirmed_holds, timed_out)" not in runtime:
    errors.append("RST sequence timeout guard missing")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("RST/RGB factory-reset contract PASS")
