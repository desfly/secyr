#pragma once

#include "homeguard/device_api_model.hpp"

#include <deque>
#include <string>

namespace homeguard {

class DeviceCommandRouter {
public:
    explicit DeviceCommandRouter(DeviceApiState& state);

    DeviceCommandResponse handle(const DeviceCommandRequest& request);

private:
    bool is_duplicate(const std::string& request_id) const;
    void remember(const std::string& request_id);

    DeviceApiState& state_;
    std::deque<std::string> recent_request_ids_;
};

}  // namespace homeguard
