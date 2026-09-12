#pragma once

#include "homeguard/ble_remote.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hg {
class BootReadinessReport;
class PhysicalOutputRuntime;
class SystemEventBus;
class SystemModel;
}

namespace homeguard::idf {

class BleRemoteRuntime {
public:
    static constexpr std::uint32_t pairing_window_ms = 30'000U;
    static constexpr std::uint32_t lock_pulse_ms = 5'000U;

    void configure(
        hg::SystemModel* model,
        hg::BootReadinessReport* readiness,
        hg::PhysicalOutputRuntime* physical,
        hg::SystemEventBus* bus);

    bool begin_pairing(std::string_view name, std::uint32_t permissions, std::uint64_t now_ms);
    void cancel_pairing();
    [[nodiscard]] bool pairing_active(std::uint64_t now_ms) const;
    [[nodiscard]] std::size_t size() const { return registry_.size(); }

    hg::BleRemoteResult ingest(const hg::BleRemoteEvent& event, std::uint64_t now_ms);
    void tick(std::uint64_t now_ms);

private:
    bool execute(hg::BleRemoteAction action, std::uint64_t now_ms);
    void publish_remote_event(const hg::BleRemoteResult& result, hg::BleRemoteAction action, std::uint64_t now_ms);

    hg::BleRemoteRegistry registry_{};
    hg::SystemModel* model_{};
    hg::BootReadinessReport* readiness_{};
    hg::PhysicalOutputRuntime* physical_{};
    hg::SystemEventBus* bus_{};

    std::uint64_t pairing_deadline_ms_{};
    std::uint32_t pairing_permissions_{};
    char pairing_name_[24]{};

    std::uint64_t lock_deadline_ms_{};
};

}  // namespace homeguard::idf
