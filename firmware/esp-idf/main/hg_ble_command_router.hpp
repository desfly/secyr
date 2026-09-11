#pragma once

#include "homeguard/provisioning.hpp"

#include <cstdint>
#include <string>

class NvsConfigStore;

namespace hg {
class BootReadinessReport;
class PhysicalOutputRuntime;
class SystemEventBus;
class SystemModel;
}

namespace homeguard {
class AccessControl;
}

namespace homeguard::idf {

class BleTransport;
class NetworkHttp;

class BleCommandRouter {
public:
    void configure(
        BleTransport* transport,
        homeguard::AccessControl* access,
        hg::SystemModel* model,
        hg::BootReadinessReport* readiness,
        hg::PhysicalOutputRuntime* physical,
        hg::SystemEventBus* bus,
        NetworkHttp* network,
        NvsConfigStore* provisioning_store);

    void handle(std::uint8_t type, const std::string& json);

private:
    bool session_valid() const;
    void handle_hello(const std::string& json);
    void handle_command(const std::string& json);
    void handle_provisioning_authorize(const std::string& json);
    void handle_provisioning_apply(const std::string& json);
    bool prepare_provisioning_session(std::uint64_t now_ms);
    void send(std::uint8_t type, const std::string& json) const;
    void send_error(const char* reason) const;
    void send_provisioning_reply(bool ok, const char* stage, const char* reason = nullptr) const;

    BleTransport* transport_{};
    homeguard::AccessControl* access_{};
    hg::SystemModel* model_{};
    hg::BootReadinessReport* readiness_{};
    hg::PhysicalOutputRuntime* physical_{};
    hg::SystemEventBus* bus_{};
    NetworkHttp* network_{};
    NvsConfigStore* provisioning_store_{};
    hg::ProvisioningSession provisioning_session_{};
    bool provisioning_session_prepared_{};
    std::uint32_t provisioning_epoch_{};
    std::string actor_{};
    std::uint32_t authenticated_epoch_{};
};

}  // namespace homeguard::idf
