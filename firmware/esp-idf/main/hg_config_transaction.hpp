#pragma once

#include "hg_access_nvs.hpp"
#include "hg_cloud_nvs.hpp"
#include "hg_commissioning_nvs.hpp"
#include "hg_config_backup.hpp"
#include "hg_network_http.hpp"

#include <string>

namespace homeguard::idf {

class ConfigTransaction {
public:
    ConfigTransaction(
        const AccessNvsStore& access_store,
        const NetworkHttp& network,
        const CloudNvsStore& cloud_store,
        const CommissioningNvsStore& commissioning_store)
        : access_store_(access_store),
          network_(network),
          cloud_store_(cloud_store),
          commissioning_store_(commissioning_store) {}

    // Applies only already-decoded/validated ConfigBackupV1 data.
    // On any write failure the previous persistent state is restored before
    // returning false. A rollback failure is reported separately in reason.
    [[nodiscard]] bool apply(const ConfigBackupV1& backup, std::string& reason) const;

private:
    struct Snapshot {
        bool access_present{};
        AccessControl access;

        bool wifi_present{};
        std::string wifi_ssid;
        std::string wifi_password;

        bool cloud_present{};
        CloudConfig cloud;

        bool commissioning_present{};
        hg::CommissioningPersistentState commissioning;
    };

    [[nodiscard]] bool capture(Snapshot& snapshot, std::string& reason) const;
    [[nodiscard]] bool restore(const Snapshot& snapshot) const;

    const AccessNvsStore& access_store_;
    const NetworkHttp& network_;
    const CloudNvsStore& cloud_store_;
    const CommissioningNvsStore& commissioning_store_;
};

}  // namespace homeguard::idf
