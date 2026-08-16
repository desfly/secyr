#pragma once

#include "homeguard/boot_readiness.hpp"
#include "homeguard/commissioning_state.hpp"
#include "homeguard/hardware_verification.hpp"
#include "homeguard/system_model.hpp"

#include <cstdint>
#include <mutex>

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

enum class PhysicalInputChannel : int {
    ColdValveOpenLimit = 0,
    ColdValveClosedLimit = 1,
    HotValveOpenLimit = 2,
    HotValveClosedLimit = 3,
    EnclosureTamper = 4,
    ExternalService = 5,
    Reserve1 = 6,
    Reserve2 = 7,
};

class PhysicalOutputBackend {
public:
    virtual ~PhysicalOutputBackend() = default;
    virtual bool configure_output(int channel, bool initial_level) = 0;
    virtual bool write_output(int channel, bool level) = 0;
    // Backends without input capability remain fail-closed for supervised
    // actuator movement. HW-678's MCP23017 backend overrides this.
    virtual bool read_inputs(std::uint8_t* value) { (void)value; return false; }
};

enum class PhysicalOutputStatus : std::uint8_t {
    Ready,
    FailClosed,
    InvalidHardware,
    BackendError,
    ValveSafetyFault,
    ValveTimeout,
};

enum class ValveMotionDirection : std::uint8_t {
    Stopped,
    Opening,
    Closing,
};

struct ValveMotionState {
    ValveMotionDirection direction{ValveMotionDirection::Stopped};
    std::uint32_t command_revision{};
    std::uint64_t started_at_ms{};
    std::uint32_t timeout_ms{};
};

struct PhysicalOutputRuntimeState {
    PhysicalOutputStatus status{PhysicalOutputStatus::FailClosed};
    bool outputs_enabled{};
    bool safety_fault_latched{};
    std::uint32_t writes{};
    std::uint32_t failures{};
    std::uint32_t limit_stops{};
    std::uint32_t valve_timeouts{};
    ValveMotionState cold_valve{};
    ValveMotionState hot_valve{};
};

class PhysicalOutputRuntime {
public:
    bool initialize(
        PhysicalOutputBackend& backend,
        const HardwareVerificationRecord& hardware,
        const CommissioningPersistentState& commissioning,
        const BootReadinessReport& readiness);

    // Called by the dedicated output supervisor. Each output command revision is
    // consumed once; valve motion is then independently stopped by end-switch or
    // measured commissioning timeout even if no further HTTP/cloud command arrives.
    bool synchronize(
        const SystemModel& model,
        const BootReadinessReport& readiness,
        std::uint64_t now_ms);
    bool force_safe();

    [[nodiscard]] PhysicalOutputRuntimeState state() const;

private:
    struct LimitSnapshot {
        bool cold_open{};
        bool cold_closed{};
        bool hot_open{};
        bool hot_closed{};
    };

    bool write_safe_locked(PhysicalOutputChannel channel);
    bool write_logical_locked(PhysicalOutputChannel channel, bool active);
    bool force_safe_locked();
    bool read_limits_locked(LimitSnapshot& limits);
    bool process_valve_locked(
        ValveMotionState& motion,
        PhysicalOutputChannel open_channel,
        PhysicalOutputChannel close_channel,
        bool open_limit,
        bool close_limit,
        const OutputRecord* output,
        std::uint32_t configured_timeout_ms,
        std::uint64_t now_ms);
    bool stop_valve_locked(
        ValveMotionState& motion,
        PhysicalOutputChannel open_channel,
        PhysicalOutputChannel close_channel,
        bool reached_limit);
    bool latch_fault_locked(PhysicalOutputStatus status);

    PhysicalOutputBackend* backend_{};
    const CommissioningPersistentState* commissioning_{};
    mutable std::mutex mutex_;
    PhysicalOutputRuntimeState state_{};
    bool siren_known_{};
    bool siren_active_{};
};

[[nodiscard]] const char* to_string(PhysicalOutputStatus status);
[[nodiscard]] const char* to_string(ValveMotionDirection direction);

}  // namespace hg
