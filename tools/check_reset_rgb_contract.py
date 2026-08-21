from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "firmware" / "esp-idf" / "main" / "hg_reset_sequence.cpp"
HEADER = ROOT / "firmware" / "include" / "homeguard" / "reset_sequence.hpp"

runtime = RUNTIME.read_text(encoding="utf-8")
header = HEADER.read_text(encoding="utf-8")
errors = []

# Approved HomeGuard-S3 physical RST/EN contract:
# 3 accepted steps + no continuation -> settings reset, users preserved, WHITE
# 5 accepted steps -> full user factory reset, RED.
for expected in (
    "kSettingsResetRequiredRstPresses = 3U",
    "kFactoryResetRequiredRstPresses = 5U",
    "kFactoryResetStepWhiteMs = 1500U",
    "kFactoryResetSequenceWindowMs = 5000U",
    "kSettingsResetSuccessWhiteMs = 5000U",
    "kFactoryResetSuccessRedMs = 5000U",
):
    if expected not in header:
        errors.append(f"physical RST contract changed: missing {expected}")

for forbidden in (
    "board::kServiceButton",
    "service_button_pressed",
    "gpio_get_level(",
    "service_button_reset_task",
):
    if forbidden in runtime:
        errors.append(f"physical RST path regressed to GPIO service button: {forbidden}")

# Hardware validation proved HW-678 RST/EN is POWERON and RTC_NOINIT does not
# survive it. Keep the persistent NVS baseline and reject the old RTC approach.
for forbidden in ("RTC_NOINIT_ATTR", "g_rst_boot_marker", "rtc_state_was_valid"):
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
    "Persistent RST boot baseline established",
    "load_sequence_count(previous_count)",
    "store_sequence_count(step.count)",
    "step_feedback_and_timeout_task",
):
    if required not in runtime:
        errors.append(f"persistent physical RST detector incomplete: missing {required}")

# Every accepted step must visibly acknowledge WHITE before its progress is
# committed. An RGB failure cancels the sequence.
white = runtime.find("RgbDiagnostic::set_white(board::kOnboardRgb)", runtime.find("handle_physical_rst_factory_reset()"))
white_error = runtime.find("Cannot show WHITE physical-RST acknowledgement", white)
persist = runtime.find("store_sequence_count(step.count)", white)
if min(white, white_error, persist) < 0 or not (white < white_error < persist):
    errors.append("physical RST step must show WHITE successfully before persistence")

# Step 3 is not destructive immediately. It is persisted, then the timeout
# worker may stage Settings reset only if no step 4 arrives within the window.
third_log = runtime.find("step 3/5; Settings Reset armed if sequence stops here")
settings_timeout = runtime.find("count == hg::kSettingsResetRequiredRstPresses")
settings_stage = runtime.find("set_pending_reset(PendingReset::Settings)", settings_timeout)
settings_restart = runtime.find("esp_restart();", settings_stage)
if min(third_log, settings_timeout, settings_stage, settings_restart) < 0:
    errors.append("three-step delayed Settings Reset path is incomplete")

# Step 4 must be non-destructive and time out to no reset if step 5 never comes.
if "factory extension timed out after step 4/5; no reset performed" not in runtime:
    errors.append("step 4 must time out safely without reset")

# Step 5 must select and stage full Factory Reset after WHITE acknowledgement.
fifth = runtime.find("step 5/5; full Factory Reset selected")
fifth_delay = runtime.find("vTaskDelay(kStepWhiteTicks);", fifth)
fifth_off = runtime.find("RgbDiagnostic::off(board::kOnboardRgb)", fifth_delay)
stage_factory = runtime.find("stage_factory_reset_request()", fifth_off)
fifth_restart = runtime.find("esp_restart();", stage_factory)
if min(fifth, fifth_delay, fifth_off, stage_factory, fifth_restart) < 0 or not (
    fifth < fifth_delay < fifth_off < stage_factory < fifth_restart
):
    errors.append("fifth RST must be WHITE -> OFF -> stage full Factory Reset -> reboot")

# Settings reset success is WHITE for 5 seconds.
settings_erase = runtime.find("FactoryResetManager{}.erase_settings_state()")
settings_complete = runtime.find("Settings Reset complete; WHITE RGB confirmation for 5 seconds", settings_erase)
settings_white = runtime.find("RgbDiagnostic::set_white(board::kOnboardRgb)", settings_complete)
settings_delay = runtime.find("vTaskDelay(kSettingsSuccessWhiteTicks);", settings_white)
if min(settings_erase, settings_complete, settings_white, settings_delay) < 0:
    errors.append("successful Settings Reset WHITE confirmation path is incomplete")

# Full factory success is RED for 5 seconds.
factory_erase = runtime.find("FactoryResetManager{}.erase_mutable_state()")
factory_complete = runtime.find("Factory Reset complete; RED RGB confirmation for 5 seconds", factory_erase)
factory_red = runtime.find("RgbDiagnostic::set_red(board::kOnboardRgb)", factory_complete)
factory_delay = runtime.find("vTaskDelay(kFactorySuccessRedTicks);", factory_red)
if min(factory_erase, factory_complete, factory_red, factory_delay) < 0:
    errors.append("successful full Factory Reset RED confirmation path is incomplete")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Physical RST/RGB 3-step settings + 5-step factory contract PASS")
