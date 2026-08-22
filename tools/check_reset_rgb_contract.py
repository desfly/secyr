from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "firmware" / "esp-idf" / "main" / "hg_reset_sequence.cpp"
HEADER = ROOT / "firmware" / "include" / "homeguard" / "reset_sequence.hpp"

runtime = RUNTIME.read_text(encoding="utf-8")
header = HEADER.read_text(encoding="utf-8")
errors = []

# Approved HomeGuard-S3 reset contract: physical RST/EN only, three accepted
# steps, WHITE acknowledgement for each step, successful Factory Reset -> RED
# for five seconds -> OFF -> reboot.
for expected in (
    "kFactoryResetRequiredRstPresses = 3U",
    "kFactoryResetStepWhiteMs = 1500U",
    "kFactoryResetSequenceWindowMs = 5000U",
    "kFactoryResetSuccessRedMs = 5000U",
):
    if expected not in header:
        errors.append(f"physical RST contract changed: missing {expected}")

# GPIO21/service-button substitution is specifically forbidden in this path.
for forbidden in (
    "board::kServiceButton",
    "service_button_pressed",
    "gpio_get_level(",
    "service_button_reset_task",
):
    if forbidden in runtime:
        errors.append(f"physical RST path regressed to GPIO service button: {forbidden}")

# Hardware validation on HW-678 proved that RST/EN is reported as POWERON and
# RTC_NOINIT does NOT survive that reset. The detector must therefore use a
# persistent NVS boot baseline, never the disproved RTC marker approach.
for forbidden in (
    "RTC_NOINIT_ATTR",
    "g_rst_boot_marker",
    "rtc_state_was_valid",
):
    if forbidden in runtime:
        errors.append(f"physical RST detector regressed to invalid RTC assumption: {forbidden}")

for required in (
    'kSequenceNamespace = "hg_rstseq"',
    'kBootMarkerKey = "boot_seen"',
    "load_boot_marker(boot_marker_was_valid)",
    "store_boot_marker()",
    "esp_reset_reason()",
    "ESP_RST_EXT",
    "ESP_RST_POWERON",
    "hg::reset_press_detected(",
    "boot_marker_was_valid",
    "Persistent RST boot baseline established",
):
    if required not in runtime:
        errors.append(f"persistent physical RST detector incomplete: missing {required}")

# A newly flashed build ends with an esptool EN/RTS reset, which HW-678 also
# reports as POWERON. That first boot must establish a firmware-specific NVS
# baseline and return before any WHITE acknowledgement can be emitted.
for required in (
    'kFirmwareSignatureKey = "fw_sig"',
    "firmware_signature(HG_GIT_REVISION)",
    "load_firmware_signature(firmware_signature_matches)",
    "store_firmware_signature()",
    "if (!firmware_signature_matches)",
    "WHITE stays off",
):
    if required not in runtime:
        errors.append(f"firmware-flash RST baseline incomplete: missing {required}")

signature_guard = runtime.find("if (!firmware_signature_matches)")
signature_return = runtime.find("return false;", signature_guard)
white = runtime.find("RgbDiagnostic::set_white(board::kOnboardRgb)")
if min(signature_guard, signature_return, white) < 0:
    errors.append("firmware-flash baseline ordering is incomplete")
elif not (signature_guard < signature_return < white):
    errors.append("new firmware baseline must return before WHITE RST acknowledgement")

# Progress must persist across the reset itself, but an abandoned sequence must
# expire. This is what lets the hardware RST/EN button be used safely.
for required in (
    "load_sequence_count(previous_count)",
    "store_sequence_count(step.count)",
    "step_feedback_and_timeout_task",
    "store_sequence_count(0U)",
):
    if required not in runtime:
        errors.append(f"physical RST sequence persistence/timeout missing: {required}")

# WHITE must be successfully shown before a non-final step is persisted. If the
# RGB driver fails, the sequence is cancelled instead of silently counting it.
white = runtime.find("RgbDiagnostic::set_white(board::kOnboardRgb)")
white_error = runtime.find("Cannot show WHITE physical-RST acknowledgement", white)
persist = runtime.find("store_sequence_count(step.count)", white)
if min(white, white_error, persist) < 0:
    errors.append("WHITE RST-step acknowledgement path is incomplete")
elif not (white < white_error < persist):
    errors.append("physical RST step may persist only after WHITE acknowledgement succeeds")

# The third accepted step must also acknowledge WHITE, then stage destructive
# work for the next safe early boot rather than erasing live runtime state.
third = runtime.find('Physical RST accepted: WHITE acknowledgement, step 3/3')
third_delay = runtime.find("vTaskDelay(kStepWhiteTicks);", third)
third_off = runtime.find("RgbDiagnostic::off(board::kOnboardRgb)", third_delay)
stage = runtime.find("stage_factory_reset_request()", third_off)
third_restart = runtime.find("esp_restart();", stage)
if min(third, third_delay, third_off, stage, third_restart) < 0:
    errors.append("third physical RST staging path is incomplete")
elif not (third < third_delay < third_off < stage < third_restart):
    errors.append("third RST must be WHITE -> delay -> OFF -> stage Factory Reset -> reboot")

# A successful complete erase is confirmed locally by RED for exactly the
# contract duration. RED is never used as the per-step acknowledgement.
erase = runtime.find("FactoryResetManager{}.erase_mutable_state()")
consume_pending = runtime.find("set_pending_reset(false)", erase)
red = runtime.find("RgbDiagnostic::set_red(board::kOnboardRgb)", consume_pending)
red_delay = runtime.find("vTaskDelay(kSuccessRedTicks);", red)
red_off = runtime.find("RgbDiagnostic::off(board::kOnboardRgb)", red_delay)
red_restart = runtime.find("esp_restart();", red_off)
if min(erase, consume_pending, red, red_delay, red_off, red_restart) < 0:
    errors.append("RED successful-reset confirmation path is incomplete")
elif not (erase < consume_pending < red < red_delay < red_off < red_restart):
    errors.append("successful Factory Reset must be erase -> consume pending -> RED -> 5 s -> OFF -> reboot")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Physical RST/RGB factory-reset contract PASS")
