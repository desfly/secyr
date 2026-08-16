#pragma once

#include "hg_cloud_nvs.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/commissioning_state.hpp"

#include <string>
#include <string_view>

namespace homeguard::idf {

struct ConfigBackupV1 {
    static constexpr int version = 1;

    bool secrets_included{};
    homeguard::AccessControl access;

    bool wifi_present{};
    std::string wifi_ssid;
    std::string wifi_password;

    bool cloud_present{};
    CloudConfig cloud;

    bool commissioning_present{};
    hg::CommissioningPersistentState commissioning;
};

class ConfigBackupV1Codec {
public:
    [[nodiscard]] static std::string encode(const ConfigBackupV1& backup);
    [[nodiscard]] static bool decode(std::string_view json, ConfigBackupV1& backup, std::string& reason);
};

}  // namespace homeguard::idf
