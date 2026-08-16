#include "hg_config_transaction.hpp"

#include "nvs.h"

namespace homeguard::idf {

bool ConfigTransaction::capture(Snapshot& snapshot, std::string& reason) const
{
    reason.clear();

    auto error = access_store_.load(snapshot.access);
    if (error == ESP_OK) {
        snapshot.access_present = true;
    } else if (error == ESP_ERR_NVS_NOT_FOUND) {
        snapshot.access_present = false;
    } else {
        reason = "snapshot_access_failed";
        return false;
    }

    error = network_.snapshot_persisted_credentials(
        snapshot.wifi_ssid,
        snapshot.wifi_password,
        snapshot.wifi_present);
    if (error != ESP_OK) {
        reason = "snapshot_wifi_failed";
        return false;
    }

    error = cloud_store_.load(snapshot.cloud);
    if (error == ESP_OK) {
        snapshot.cloud_present = true;
    } else if (error == ESP_ERR_NVS_NOT_FOUND) {
        snapshot.cloud_present = false;
    } else {
        reason = "snapshot_cloud_failed";
        return false;
    }

    error = commissioning_store_.load_commissioning(snapshot.commissioning);
    if (error == ESP_OK) {
        snapshot.commissioning_present = true;
    } else if (error == ESP_ERR_NVS_NOT_FOUND) {
        snapshot.commissioning_present = false;
    } else {
        reason = "snapshot_commissioning_failed";
        return false;
    }

    return true;
}

bool ConfigTransaction::restore(const Snapshot& snapshot) const
{
    bool ok = true;

    if (snapshot.access_present) {
        ok = access_store_.save(snapshot.access) == ESP_OK && ok;
    } else {
        ok = access_store_.erase() == ESP_OK && ok;
    }

    if (snapshot.wifi_present) {
        ok = network_.save_persisted_credentials(snapshot.wifi_ssid, snapshot.wifi_password) && ok;
    } else {
        ok = network_.clear_persisted_credentials() && ok;
    }

    if (snapshot.cloud_present) {
        ok = cloud_store_.save(snapshot.cloud) == ESP_OK && ok;
    } else {
        ok = cloud_store_.clear() == ESP_OK && ok;
    }

    if (snapshot.commissioning_present) {
        ok = commissioning_store_.save_commissioning(snapshot.commissioning) == ESP_OK && ok;
    } else {
        ok = commissioning_store_.erase_commissioning_state() == ESP_OK && ok;
    }

    return ok;
}

bool ConfigTransaction::apply(const ConfigBackupV1& backup, std::string& reason) const
{
    reason.clear();

    // Defense in depth: even callers that bypass ConfigBackupV1Codec must not
    // persist an access database with no enabled Admin. Such a DB would exist
    // after reboot (keeping first-Admin bootstrap disabled) but provide no
    // credential capable of repairing controller configuration.
    if (backup.access.enabled_admin_count() == 0U) {
        reason = "access_admin_required";
        return false;
    }

    Snapshot before{};
    if (!capture(before, reason)) return false;

    auto rollback = [&](const char* write_reason) {
        if (!restore(before)) {
            reason = std::string{write_reason} + ":rollback_failed";
        } else {
            reason = write_reason;
        }
        return false;
    };

    if (access_store_.save(backup.access) != ESP_OK) {
        return rollback("write_access_failed");
    }

    if (backup.wifi_present) {
        if (!network_.save_persisted_credentials(backup.wifi_ssid, backup.wifi_password)) {
            return rollback("write_wifi_failed");
        }
    } else if (!network_.clear_persisted_credentials()) {
        return rollback("clear_wifi_failed");
    }

    if (backup.cloud_present) {
        if (cloud_store_.save(backup.cloud) != ESP_OK) {
            return rollback("write_cloud_failed");
        }
    } else if (cloud_store_.clear() != ESP_OK) {
        return rollback("clear_cloud_failed");
    }

    if (backup.commissioning_present) {
        if (commissioning_store_.save_commissioning(backup.commissioning) != ESP_OK) {
            return rollback("write_commissioning_failed");
        }
    } else if (commissioning_store_.erase_commissioning_state() != ESP_OK) {
        return rollback("clear_commissioning_failed");
    }

    return true;
}

}  // namespace homeguard::idf
