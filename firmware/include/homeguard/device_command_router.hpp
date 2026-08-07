#pragma once

#include "homeguard/device_api_model.hpp"

#include <deque>
#include <string>

namespace homeguard {

class AccessControl;

class DeviceCommandRouter {
public:
    explicit DeviceCommandRouter(DeviceApiState& state, AccessControl* access_control = nullptr);

    DeviceCommandResponse handle(const DeviceCommandRequest& request);

private:
    bool is_duplicate(const std::string& request_id) const;
    void remember(const std::string& request_id);

    DeviceApiState& state_;
    AccessControl* access_control_{};
    std::deque<std::string> recent_request_ids_;
};

}  // namespace homeguard
