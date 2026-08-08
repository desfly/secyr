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
