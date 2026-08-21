from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "firmware" / "esp-idf" / "main" / "hg_reset_sequence.cpp"
HEADER = ROOT / "firmware" / "include" / "homeguard" / "reset_sequence.hpp"

runtime = RUNTIME.read_text(encoding="utf-8")
header = HEADER.read_text(encoding="utf-8")
errors = []

# Normative physical RST/EN contract.
for expected in (
    "kFactoryResetRequiredRstPresses = 3U",
    "kFactoryResetWhiteAckMs = 1500U",
    "kFactoryResetSequenceWindowMs = 5000U",
    "kFactoryResetSuccessRedMs = 5000U",
):
    if expected not in header:
        errors.append(f"physical RST contract changed: missing {expected}")

# The old GPIO21 service-button replacement is forbidden here. The factory
# reset gesture must be driven by physical RST/EN reset reasons.
for forbidden in (
    "board::kServiceButton",
    "service_button_pressed",
    "gpio_get_level",
):
    if forbidden in runtime:
        errors.append(f"physical RST contract regressed to GPIO button path: {forbidden}")

for required in (
    "RTC_NOINIT_ATTR",
    "esp_reset_reason()",
    "ESP_RST_EXT",
    "ESP_RST_POWERON",
    "hg::reset_press_detected",
    "hg::advance_reset_sequence",
    "kSequenceNamespace = \"hg_rstseq\"",
):
    if required not in runtime:
        errors.append(f"physical RST detector incomplete: missing {required}")

# True cold POWERON must be distinguishable from the board RST/EN POWERON.
if "rtc_state_was_valid" not in runtime or "g_rst_boot_marker" not in runtime:
    errors.append("physical RST detector must guard POWERON with RTC boot marker")

# Every accepted physical RST step must visibly acknowledge with WHITE before
# it is persisted/counts toward destructive reset.
white_call = runtime.find("RgbDiagnostic::set_white(board::kOnboardRgb)")
white_failure = runtime.find("Cannot show WHITE physical-RST acknowledgement", white_call)
advance = runtime.find("hg::advance_reset_sequence", 0)
if min(white_call, white_failure, advance) < 0:
    errors.append("WHITE physical-RST acknowledgement path is incomplete")

if "Physical RST accepted: WHITE acknowledgement, step %u/%u" not in runtime:
    errors.append("physical RST steps must log WHITE acknowledgement")
if "Physical RST accepted: WHITE acknowledgement, step 3/3" not in runtime:
    errors.append("third physical RST WHITE acknowledgement missing")

# Incomplete sequences must self-expire instead of remaining armed forever.
if "sequence_feedback_timeout_task" not in runtime:
    errors.append("physical RST sequence timeout worker missing")
if "store_sequence_count(0U)" not in runtime:
    errors.append("physical RST sequence counter clear missing")

# Third accepted physical RST must stage reset for safe early boot and reboot.
stage = runtime.find("stage_factory_reset_request()")
restart = runtime.find("esp_restart();", stage)
if min(stage, restart) < 0 or stage >= restart:
    errors.append("triple physical RST must stage Factory Reset before reboot")

# Successful erase must be RED for exactly the approved duration, then OFF and reboot.
red_call = runtime.find("RgbDiagnostic::set_red(board::kOnboardRgb)")
red_delay = runtime.find("vTaskDelay(kSuccessRedTicks);", red_call)
red_off = runtime.find("RgbDiagnostic::off(board::kOnboardRgb)", red_delay)
red_restart = runtime.find("esp_restart();", red_off)
if min(red_call, red_delay, red_off, red_restart) < 0:
    errors.append("RED success-confirmation path is incomplete")
elif not (red_call < red_delay < red_off < red_restart):
    errors.append("Factory Reset success must be RED -> 5 s -> OFF -> reboot")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Physical RST/RGB factory-reset contract PASS")
