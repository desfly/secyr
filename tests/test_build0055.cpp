#include "test_framework.hpp"
#include "homeguard/physical_output_diagnostics.hpp"

void test_build0055() {
    hg::PhysicalOutputRuntimeState runtime{};
    runtime.status = hg::PhysicalOutputStatus::FailClosed;
    runtime.outputs_enabled = false;
    runtime.writes = 4;
    runtime.failures = 0;

    hg::BootReadinessReport blocked{};
    const auto blocked_diag = hg::make_physical_output_diagnostics(runtime, blocked);
    TEST_CHECK(blocked_diag.healthy);
    TEST_CHECK(!blocked_diag.outputs_enabled);
    TEST_CHECK(!blocked_diag.outputs_allowed);

    hg::BootReadinessReport ready{};
    ready.status = hg::BootReadinessStatus::ReadyForPhysicalOutputs;
    runtime.status = hg::PhysicalOutputStatus::Ready;
    runtime.outputs_enabled = true;
    const auto ready_diag = hg::make_physical_output_diagnostics(runtime, ready);
    TEST_CHECK(ready_diag.healthy);
    TEST_CHECK(ready_diag.outputs_enabled);
    TEST_CHECK(ready_diag.outputs_allowed);

    runtime.failures = 1;
    runtime.status = hg::PhysicalOutputStatus::BackendError;
    const auto failed_diag = hg::make_physical_output_diagnostics(runtime, ready);
    TEST_CHECK(!failed_diag.healthy);

    runtime.failures = 0;
    runtime.status = hg::PhysicalOutputStatus::Ready;
    runtime.outputs_enabled = true;
    const auto inconsistent = hg::make_physical_output_diagnostics(runtime, blocked);
    TEST_CHECK(!inconsistent.healthy);

    const auto json = hg::physical_output_diagnostics_json(failed_diag);
    TEST_CHECK(json.find("\"healthy\":false") != std::string::npos);
    TEST_CHECK(json.find("\"failures\":1") != std::string::npos);
}
