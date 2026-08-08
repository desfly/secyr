#include "test_framework.hpp"
#include "homeguard/commissioning.hpp"

void test_build0046() {
    hg::CommissioningEvaluator evaluator;

    hg::CommissioningInput base{};
    base.controller_alive = true;
    base.zones_available = true;
    base.analog_available = true;
    base.event_log_available = true;

    auto dry = evaluator.evaluate(base);
    CHECK(dry.dry_run_ready());
    CHECK(!dry.actuator_test_ready());
    CHECK(dry.state == hg::CommissioningState::DryRunReady);

    auto ready = base;
    ready.physical_outputs_available = true;
    ready.gpio_map_verified = true;
    ready.active_polarity_verified = true;
    ready.maintenance_active = true;
    auto actuator = evaluator.evaluate(ready);
    CHECK(actuator.actuator_test_ready());
    CHECK(actuator.failed == 0);

    auto armed = ready;
    armed.system_armed = true;
    CHECK(!evaluator.evaluate(armed).actuator_test_ready());

    auto alarm = ready;
    alarm.alarm_active = true;
    CHECK(!evaluator.evaluate(alarm).actuator_test_ready());

    auto no_polarity = ready;
    no_polarity.active_polarity_verified = false;
    CHECK(!evaluator.evaluate(no_polarity).actuator_test_ready());

    auto no_gpio = ready;
    no_gpio.gpio_map_verified = false;
    CHECK(!evaluator.evaluate(no_gpio).actuator_test_ready());

    auto broken_runtime = base;
    broken_runtime.zones_available = false;
    CHECK(!evaluator.evaluate(broken_runtime).dry_run_ready());

    const auto json = hg::commissioning_json(actuator);
    CHECK(json.find("actuator_test_ready") != std::string::npos);
    CHECK(json.find("gpio_map") != std::string::npos);
}
