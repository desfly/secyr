#pragma once

#include "homeguard/hardware_runtime.hpp"

#include <string>

namespace homeguard {

struct HardwareApiResponse {
    int http_status{200};
    std::string content_type{"application/json"};
    std::string body;
};

HardwareApiResponse hardware_status_response(
    const HardwareRuntimeStatus& status);

}  // namespace homeguard
