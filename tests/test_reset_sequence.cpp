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

    // HomeGuard-S3 physical RST/EN is classified as POWERON by ROM/IDF.
    CHECK(hg::physical_reset_candidate(false, true));
    CHECK(hg::physical_reset_candidate(true, false));
    CHECK(hg::physical_reset_candidate(true, true));
    CHECK(!hg::physical_reset_candidate(false, false));

    step = hg::advance_reset_sequence(
        0U,
        hg::physical_reset_candidate(false, true));
    CHECK(step.count == 1U);
    CHECK(!step.trigger_factory_reset);
}
