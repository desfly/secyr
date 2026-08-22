#include "homeguard/reset_sequence.hpp"
#include "test_framework.hpp"

void test_reset_sequence() {
    CHECK(hg::kFactoryResetRequiredRstPresses == 3U);
    CHECK(hg::kFactoryResetStepWhiteMs == 1500U);
    CHECK(hg::kFactoryResetSequenceWindowMs == 5000U);
    CHECK(hg::kFactoryResetSuccessRedMs == 5000U);

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
    CHECK(!step.trigger_factory_reset);

    step = hg::advance_reset_sequence(step.count, true);
    CHECK(step.count == 2U);
    CHECK(!step.trigger_factory_reset);

    step = hg::advance_reset_sequence(step.count, true);
    CHECK(step.count == 0U);
    CHECK(step.trigger_factory_reset);

    // A software/watchdog reset cannot advance a physical-RST sequence.
    step = hg::advance_reset_sequence(2U, false);
    CHECK(step.count == 0U);
    CHECK(!step.trigger_factory_reset);

    // Disabled sequence can never trigger destructive work.
    step = hg::advance_reset_sequence(2U, true, 0U);
    CHECK(step.count == 0U);
    CHECK(!step.trigger_factory_reset);

    // Saturated/corrupt counters trigger deterministically instead of wrapping.
    step = hg::advance_reset_sequence(0xffU, true, 3U);
    CHECK(step.count == 0U);
    CHECK(step.trigger_factory_reset);
}
