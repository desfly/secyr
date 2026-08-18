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

    // Cold POWERON must not count as a button press because RTC sequence
    // state is not yet known to be valid.
    CHECK(!hg::reset_press_detected(false, false, true));

    // On this ESP32-S3 board the physical RST/EN button is reported by the
    // ROM/IDF as POWERON. Once our RTC marker survived a previous boot, that
    // POWERON classification represents the physical reset button.
    CHECK(hg::reset_press_detected(true, false, true));
    CHECK(hg::reset_press_detected(true, true, false));
    CHECK(!hg::reset_press_detected(true, false, false));

    step = hg::advance_reset_sequence(
        0U,
        hg::reset_press_detected(true, false, true));
    CHECK(step.count == 1U);
    CHECK(!step.trigger_factory_reset);
}
