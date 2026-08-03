#include "homeguard/hardware_api.hpp"

namespace homeguard {

HardwareApiResponse hardware_status_response(
    const HardwareRuntimeStatus& status)
{
    return {
        200,
        "application/json",
        hardware_runtime_json(status),
    };
}

}  // namespace homeguard
