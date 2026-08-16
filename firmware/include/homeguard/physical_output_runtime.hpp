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
    bool maintenance_mode{};
    std::uint32_t writes{};
    std::uint32_t failures{};
    std::uint32_t limit_stops{};
    std::uint32_t valve_timeouts{};
    ValveMotionState cold_valve{};
    ValveMotionState hot_valve{};
};

using BenchDelayFn = void (*)(std::uint32_t duration_ms);

class PhysicalOutputRuntime {
public:
    static constexpr std::uint32_t kMaxBenchPulseMs = 1000U;

    bool initialize(
        PhysicalOutputBackend& backend,
        const HardwareVerificationRecord& hardware,
        const CommissioningPersistentState& commissioning,
        const BootReadinessReport& readiness);

    bool update_control_state(
        const HardwareVerificationRecord& hardware,
        const CommissioningPersistentState& commissioning,
        const BootReadinessReport& readiness);

    // Maintenance is a non-sticky service gate. Both transitions force every
    // channel OFF and consume the current SystemModel output revisions without
    // executing them. This prevents a command queued just before/during service
    // from waking up when maintenance is exited. Bench pulses are the only ON
    // path while maintenance is active.
    bool set_maintenance_mode(bool active, const SystemModel& model);

    bool synchronize(const SystemModel& model, std::uint64_t now_ms);
    bool force_safe();

    bool bench_pulse(
        PhysicalOutputChannel channel,
        std::uint32_t duration_ms,
        BenchDelayFn delay_fn);

    bool lockout_fail_closed();

    [[nodiscard]] PhysicalOutputRuntimeState state() const;

private:
    struct LimitSnapshot {
        bool cold_open{};
        bool cold_closed{};
        bool hot_open{};
        bool hot_closed{};
    };

    static bool bench_channel_allowed(PhysicalOutputChannel channel) noexcept;
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
    HardwareVerificationRecord hardware_{};
    CommissioningPersistentState commissioning_{};
    BootReadinessReport readiness_{};
    mutable std::mutex mutex_;
    PhysicalOutputRuntimeState state_{};
    bool hardware_verified_{};
    std::uint32_t siren_command_revision_{};
};

[[nodiscard]] const char* to_string(PhysicalOutputStatus status);
[[nodiscard]] const char* to_string(ValveMotionDirection direction);

}  // namespace hg
