#include "homeguard/reset_sequence.hpp"
#include "test_framework.hpp"

void test_reset_sequence() {
    auto step = hg::advance_confirmed_hold(0U, false);
    CHECK(step.count == 0U);
    CHECK(!step.trigger_factory_reset);

    step = hg::advance_confirmed_hold(0U, true);
    CHECK(step.count == 1U);
    CHECK(!step.trigger_factory_reset);

    step = hg::advance_confirmed_hold(step.count, true);
    CHECK(step.count == 2U);
    CHECK(!step.trigger_factory_reset);

    step = hg::advance_confirmed_hold(step.count, true);
    CHECK(step.count == 0U);
    CHECK(step.trigger_factory_reset);

    // A short press after one confirmed hold must not advance or clear it.
    step = hg::advance_confirmed_hold(1U, false);
    CHECK(step.count == 1U);
    CHECK(!step.trigger_factory_reset);

    CHECK(hg::expire_reset_gesture(2U, false) == 2U);
    CHECK(hg::expire_reset_gesture(2U, true) == 0U);

    // Required-hold count is explicit and remains deterministic.
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
