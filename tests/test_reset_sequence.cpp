#include "homeguard/reset_sequence.hpp"
#include "test_framework.hpp"

void test_reset_sequence() {
    auto step = hg::advance_reset_sequence(0U, true);
    CHECK(step.count == 1U);
    CHECK(!step.trigger_factory_reset);

    step = hg::advance_reset_sequence(step.count, true);
    CHECK(step.count == 2U);
    CHECK(!step.trigger_factory_reset);

    step = hg::advance_reset_sequence(step.count, true);
    CHECK(step.count == 0U);
    CHECK(step.trigger_factory_reset);

    step = hg::advance_reset_sequence(2U, false);
    CHECK(step.count == 0U);
    CHECK(!step.trigger_factory_reset);

    step = hg::advance_reset_sequence(1U, true, 3U);
    CHECK(step.count == 2U);
    CHECK(!step.trigger_factory_reset);
}
