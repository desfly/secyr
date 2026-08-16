#pragma once

#include "homeguard/boot_readiness.hpp"
#include "homeguard/hardware_verification.hpp"
#include "homeguard/system_model.hpp"

#include <cstdint>

namespace hg {

enum class PhysicalOutputChannel : int {
    CorridorLight = 0,
    Siren = 1,
    ColdValveOpen = 2,
    ColdValveClose = 3,
    HotValveOpen = 4,
    HotValveClose = 5,
    Reserve1 = 6,
    Reserve2 = 7,
};

class PhysicalOutputBackend {
public:
    virtual ~PhysicalOutputBackend() = default;
    virtual bool configure_output(int channel, bool initial_level) = 0;
    virtual bool write_output(int channel, bool level) = 0;
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
    bool write_safe(PhysicalOutputChannel channel);
    bool write_logical(PhysicalOutputChannel channel, bool active);
    bool write_valve(
        PhysicalOutputChannel open_channel,
        PhysicalOutputChannel close_channel,
        const OutputRecord* output);

    PhysicalOutputBackend* backend_{};
    PhysicalOutputRuntimeState state_{};
};

[[nodiscard]] const char* to_string(PhysicalOutputStatus status);

}  // namespace hg
