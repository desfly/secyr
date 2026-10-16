#pragma once

#include "homeguard/boot_readiness.hpp"
#include "homeguard/hardware_verification.hpp"
#include "homeguard/system_model.hpp"

#include <cstdint>

namespace hg {

class PhysicalOutputBackend {
public:
    virtual ~PhysicalOutputBackend() = default;
    virtual bool configure_output(int gpio, bool initial_level) = 0;
    virtual bool write_output(int gpio, bool level) = 0;
    virtual bool synchronize_relays(const SystemModel& model) {
        const auto light = model.output_snapshot(4);
        const auto lock = model.output_snapshot(5);
        const bool light_ok = write_output(direct_light_relay_gpio, light && light->active);
        const bool lock_ok = write_output(direct_lock_relay_gpio, lock && lock->active);
        return light_ok && lock_ok;
    }
    virtual bool force_safe_relays() {
        const bool light_ok = write_output(direct_light_relay_gpio, false);
        const bool lock_ok = write_output(direct_lock_relay_gpio, false);
        return light_ok && lock_ok;
    }
};

enum class PhysicalOutputStatus : std::uint8_t {
    Ready,
    FailClosed,
    InvalidHardware,
    BackendError,
};

struct PhysicalOutputRuntimeState {
    PhysicalOutputStatus status{PhysicalOutputStatus::FailClosed};
    bool outputs_enabled{};
    std::uint32_t writes{};
    std::uint32_t failures{};
};

class PhysicalOutputRuntime {
public:
    bool initialize(
        PhysicalOutputBackend& backend,
        const HardwareVerificationRecord& hardware,
        const BootReadinessReport& readiness);

    bool synchronize(const SystemModel& model, const BootReadinessReport& readiness);
    bool force_safe();

    [[nodiscard]] const PhysicalOutputRuntimeState& state() const { return state_; }

private:
    bool write_safe(int gpio);
    bool write_logical(int gpio, bool active);

    PhysicalOutputBackend* backend_{};
    const HardwareVerificationRecord* hardware_{};
    PhysicalOutputRuntimeState state_{};
};

[[nodiscard]] const char* to_string(PhysicalOutputStatus status);

}  // namespace hg
