#include "homeguard/reset_sequence.hpp"
#include "test_framework.hpp"

void test_reset_sequence() {
    CHECK(hg::kSettingsResetRequiredRstPresses == 3U);
    CHECK(hg::kFactoryResetRequiredRstPresses == 5U);
    CHECK(hg::kFactoryResetStepWhiteMs == 1500U);
    CHECK(hg::kFactoryResetSequenceWindowMs == 5000U);
    CHECK(hg::kSettingsResetSuccessWhiteMs == 5000U);
    CHECK(hg::kFactoryResetSuccessRedMs == 5000U);

    constexpr auto firmware_a = hg::reset_firmware_signature("firmware-a");
    constexpr auto firmware_a_again = hg::reset_firmware_signature("firmware-a");
    constexpr auto firmware_b = hg::reset_firmware_signature("firmware-b");
    CHECK(firmware_a == firmware_a_again);
    CHECK(firmware_a != firmware_b);
    CHECK(hg::firmware_baseline_changed(false, 0U, firmware_a));
    CHECK(!hg::firmware_baseline_changed(true, firmware_a, firmware_a));
    CHECK(hg::firmware_baseline_changed(true, firmware_a, firmware_b));

    // Fresh NVS has no persistent boot marker, therefore the first POWERON is
    // baseline establishment rather than a destructive-reset gesture step.
    CHECK(!hg::reset_press_detected(false, false, true));

    // HW-678 reports its physical RST/EN button as POWERON. Once the persistent
    // boot baseline exists, a later POWERON can advance the reset sequence.
    CHECK(hg::reset_press_detected(true, false, true));
    CHECK(hg::reset_press_detected(false, true, false));
    CHECK(!hg::reset_press_detected(true, false, false));

    auto step = hg::advance_reset_sequence(0U, true);
    CHECK(step.count == 1U);
    CHECK(!step.arm_settings_reset);
    CHECK(!step.trigger_factory_reset);

    step = hg::advance_reset_sequence(step.count, true);
    CHECK(step.count == 2U);
    CHECK(!step.arm_settings_reset);
    CHECK(!step.trigger_factory_reset);

    // Step 3 arms settings reset but does not destroy anything immediately;
    // runtime waits out the inter-step window so steps 4/5 can continue.
    step = hg::advance_reset_sequence(step.count, true);
    CHECK(step.count == 3U);
    CHECK(step.arm_settings_reset);
    CHECK(!step.trigger_factory_reset);

    step = hg::advance_reset_sequence(step.count, true);
    CHECK(step.count == 4U);
    CHECK(!step.arm_settings_reset);
    CHECK(!step.trigger_factory_reset);

    step = hg::advance_reset_sequence(step.count, true);
    CHECK(step.count == 0U);
    CHECK(!step.arm_settings_reset);
    CHECK(step.trigger_factory_reset);

    // A software/watchdog reset cannot advance a physical-RST sequence.
    step = hg::advance_reset_sequence(4U, false);
    CHECK(step.count == 0U);
    CHECK(!step.arm_settings_reset);
    CHECK(!step.trigger_factory_reset);

    // Invalid thresholds can never trigger destructive work.
    step = hg::advance_reset_sequence(4U, true, 0U, 5U);
    CHECK(step.count == 0U);
    CHECK(!step.arm_settings_reset);
    CHECK(!step.trigger_factory_reset);

    step = hg::advance_reset_sequence(4U, true, 5U, 5U);
    CHECK(step.count == 0U);
    CHECK(!step.arm_settings_reset);
    CHECK(!step.trigger_factory_reset);

    // Saturated/corrupt counters fail deterministically into the more protected
    // full-factory path instead of wrapping through a lower reset level.
    step = hg::advance_reset_sequence(0xffU, true, 3U, 5U);
    CHECK(step.count == 0U);
    CHECK(!step.arm_settings_reset);
    CHECK(step.trigger_factory_reset);
}
