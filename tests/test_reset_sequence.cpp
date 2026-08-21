#include "homeguard/reset_sequence.hpp"
#include "test_framework.hpp"

void test_reset_sequence() {
    CHECK(hg::kFactoryResetRequiredRstPresses == 3U);
    CHECK(hg::kFactoryResetWhiteAckMs == 1500U);
    CHECK(hg::kFactoryResetSequenceWindowMs == 5000U);
    CHECK(hg::kFactoryResetSuccessRedMs == 5000U);

    // True cold POWERON is not a physical RST gesture step.
    CHECK(!hg::reset_press_detected(false, false, true));

    // This HomeGuard-S3 board reports the physical RST/EN button as POWERON;
    // once the RTC marker is valid, that reset must count.
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

    // Non-physical resets cannot make progress toward destructive reset.
    step = hg::advance_reset_sequence(2U, false);
    CHECK(step.count == 0U);
    CHECK(!step.trigger_factory_reset);

    // Disabled gesture cannot trigger destructive work.
    step = hg::advance_reset_sequence(2U, true, 0U);
    CHECK(step.count == 0U);
    CHECK(!step.trigger_factory_reset);

    // Saturated/corrupt counters fail deterministically into a single trigger.
    step = hg::advance_reset_sequence(0xffU, true, 3U);
    CHECK(step.count == 0U);
    CHECK(step.trigger_factory_reset);
}
