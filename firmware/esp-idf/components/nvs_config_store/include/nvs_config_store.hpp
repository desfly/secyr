#pragma once
#include "homeguard/controller.hpp"
#include "homeguard/provisioning.hpp"
#include <string>

struct FactoryProvisioningIdentity {
    std::string certificate_pem;
    std::string private_key_pem;
    std::string certificate_sha256;
    std::string pairing_code;
    std::string setup_password;
    [[nodiscard]] bool valid() const;
    void clear_private_material();
};

class NvsConfigStore {
public:
    bool load(hg::ControllerConfig& config);
    bool save(const hg::ControllerConfig& config);
    bool is_provisioned() const;
    bool load_provisioning(hg::ProvisioningPayload& payload) const;
    bool save_provisioning(const hg::ProvisioningPayload& payload);
    bool erase_provisioning(bool physical_presence);
    bool load_factory_identity(FactoryProvisioningIdentity& identity) const;
};
