#pragma once

#include <cstdint>
#include <string>

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

class BleCommandRouter {
public:
    void configure(
        BleTransport* transport,
        homeguard::AccessControl* access,
        hg::SystemModel* model,
        hg::BootReadinessReport* readiness,
        hg::PhysicalOutputRuntime* physical,
        hg::SystemEventBus* bus);

    void handle(std::uint8_t type, const std::string& json);

private:
    bool session_valid() const;
    void handle_hello(const std::string& json);
    void handle_command(const std::string& json);
    void send(std::uint8_t type, const std::string& json) const;
    void send_error(const char* reason) const;

    BleTransport* transport_{};
    homeguard::AccessControl* access_{};
    hg::SystemModel* model_{};
    hg::BootReadinessReport* readiness_{};
    hg::PhysicalOutputRuntime* physical_{};
    hg::SystemEventBus* bus_{};
    std::string actor_{};
    std::uint32_t authenticated_epoch_{};
};

}  // namespace homeguard::idf
