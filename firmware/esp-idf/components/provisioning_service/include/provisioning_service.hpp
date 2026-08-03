#pragma once
#include "homeguard/device_identity.hpp"
#include "homeguard/provisioning.hpp"
#include "homeguard/provisioning_qr.hpp"
#include "nvs_config_store.hpp"
#include "setup_ap.hpp"
#include <string>
#include <string_view>

struct ProvisioningBootstrap {
    std::string certificate_sha256;
    std::string pairing_code;
    std::string setup_password;
    std::string setup_url{"https://192.168.4.1:8443"};
    uint8_t channel{6};
};

class ProvisioningService {
public:
    ProvisioningService(const hg::DeviceIdentity& identity, NvsConfigStore& store, SetupAp& setup_ap);
    bool begin(const ProvisioningBootstrap& bootstrap, uint64_t now_ms);
    hg::ProvisioningCode authorize(std::string_view pairing_code, std::string_view certificate_sha256, uint64_t now_ms);
    hg::ProvisioningCode apply(hg::ProvisioningPayload payload, uint64_t now_ms);
    bool factory_reset(bool physical_presence);
    void tick(uint64_t now_ms);
    void stop();
    [[nodiscard]] bool active() const { return setup_ap_.active(); }
    [[nodiscard]] const std::string& device_id() const { return device_id_; }
    [[nodiscard]] const std::string& qr_uri() const { return qr_uri_; }
    [[nodiscard]] hg::ProvisioningStatus status(uint64_t now_ms) const { return session_.status(now_ms); }
private:
    const hg::DeviceIdentity& identity_;
    NvsConfigStore& store_;
    SetupAp& setup_ap_;
    hg::ProvisioningSession session_;
    std::string device_id_;
    std::string qr_uri_;
    hg::ProvisioningShutdownGate shutdown_gate_{};
};
