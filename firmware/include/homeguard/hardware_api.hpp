#pragma once

#include "homeguard/hardware_runtime.hpp"
#include "homeguard/hardware_test.hpp"

#include <string>

namespace homeguard {

struct HardwareApiResponse {
    int http_status{200};
    std::string content_type{"application/json"};
    std::string body;
};

HardwareApiResponse hardware_status_response(
    const HardwareRuntimeStatus& status);

HardwareApiResponse hardware_test_readiness_response(
    const hg::HardwareReadinessReport& report);

HardwareApiResponse hardware_test_result_response(
    const hg::HardwareTestResult& result);

}  // namespace homeguard
