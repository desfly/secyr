#pragma once

#include "homeguard/config.hpp"

#include <string>
#include <string_view>

namespace hg {

struct ControllerConfigBackup {
    static constexpr int version = 1;
    static constexpr std::string_view format = "homeguard-s3-controller-settings";

    static std::string encode(const ControllerConfig& config);
    static bool decode(std::string_view json, ControllerConfig& config, std::string& error);
};

}  // namespace hg
