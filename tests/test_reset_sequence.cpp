#include "homeguard/reset_sequence.hpp"
#include "test_framework.hpp"

void test_reset_sequence() {
    // Cement the agreed physical RST/RGB contract.
    CHECK(hg::kFactoryResetRequiredHolds == 3U);
    CHECK(hg::kFactoryResetHoldMs == 1500U);
    CHECK(hg::kFactoryResetSequenceTimeoutMs == 5000U);
    CHECK(hg::kFactoryResetSuccessWhiteMs == 5000U);

    // Short press: ignored, no progress toward destructive reset.
    auto step = hg::advance_confirmed_hold(0U, false);
    CHECK(step.count == 0U);
    CHECK(!step.trigger_factory_reset);

    // First RED-confirmed hold followed by release: 1/3.
    step = hg::advance_confirmed_hold(0U, true);
    CHECK(step.count == 1U);
    CHECK(!step.trigger_factory_reset);

    // Second RED-confirmed hold followed by release: 2/3.
    step = hg::advance_confirmed_hold(step.count, true);
    CHECK(step.count == 2U);
    CHECK(!step.trigger_factory_reset);

    // Third RED-confirmed hold followed by release: trigger exactly once.
    step = hg::advance_confirmed_hold(step.count, true);
    CHECK(step.count == 0U);
    CHECK(step.trigger_factory_reset);

    // A short press after one confirmed hold must not advance or clear it.
    step = hg::advance_confirmed_hold(1U, false);
    CHECK(step.count == 1U);
    CHECK(!step.trigger_factory_reset);

    // Timeout between confirmed releases cancels the unfinished sequence.
    CHECK(hg::expire_reset_gesture(2U, false) == 2U);
    CHECK(hg::expire_reset_gesture(2U, true) == 0U);

    // Required-hold count remains deterministic when explicitly overridden.
    step = hg::advance_confirmed_hold(1U, true, 2U);
    CHECK(step.count == 0U);
    CHECK(step.trigger_factory_reset);

    // A disabled gesture can never trigger destructive work.
    step = hg::advance_confirmed_hold(2U, true, 0U);
    CHECK(step.count == 2U);
    CHECK(!step.trigger_factory_reset);

    // Saturated/corrupt counters fail deterministically into a reset trigger
    // once a confirmed hold arrives, rather than wrapping back to zero silently.
    step = hg::advance_confirmed_hold(0xffU, true, 3U);
    CHECK(step.count == 0U);
    CHECK(step.trigger_factory_reset);
}
