#include "homeguard/physical_output_runtime.hpp"

#include <array>

namespace hg {
namespace {

constexpr int channel_number(PhysicalOutputChannel channel) noexcept
{
    return static_cast<int>(channel);
}

constexpr std::array<PhysicalOutputChannel, 8> kAllChannels{
    PhysicalOutputChannel::CorridorLight,
    PhysicalOutputChannel::Siren,
    PhysicalOutputChannel::ColdValveOpen,
    PhysicalOutputChannel::ColdValveClose,
    PhysicalOutputChannel::HotValveOpen,
    PhysicalOutputChannel::HotValveClose,
    PhysicalOutputChannel::Reserve1,
    PhysicalOutputChannel::Reserve2,
};

}  // namespace

bool PhysicalOutputRuntime::initialize(
    PhysicalOutputBackend& backend,
    const HardwareVerificationRecord& hardware,
    const BootReadinessReport& readiness)
{
    backend_ = &backend;
    state_ = {};

    if (!hardware_verification_allows_outputs(hardware)) {
        state_.status = PhysicalOutputStatus::InvalidHardware;
        return false;
    }

    // HW-678 actuator outputs live on MCP23017 Port A. Configure every logical
    // channel OFF before the readiness gate can ever expose physical control.
    for (const auto channel : kAllChannels) {
        if (!backend_->configure_output(channel_number(channel), false)) {
            ++state_.failures;
            state_.status = PhysicalOutputStatus::BackendError;
            force_safe();
            return false;
        }
    }

    if (!readiness.outputs_allowed()) {
        force_safe();
        state_.status = PhysicalOutputStatus::FailClosed;
        return true;
    }

    state_.outputs_enabled = true;
    state_.status = PhysicalOutputStatus::Ready;
    return true;
}

bool PhysicalOutputRuntime::write_safe(PhysicalOutputChannel channel)
{
    if (backend_ == nullptr || !backend_->write_output(channel_number(channel), false)) {
        ++state_.failures;
        return false;
    }
    ++state_.writes;
    return true;
}

bool PhysicalOutputRuntime::write_logical(PhysicalOutputChannel channel, bool active)
{
    if (backend_ == nullptr || !backend_->write_output(channel_number(channel), active)) {
        ++state_.failures;
        state_.status = PhysicalOutputStatus::BackendError;
        state_.outputs_enabled = false;
        return false;
    }
    ++state_.writes;
    return true;
}

bool PhysicalOutputRuntime::write_valve(
    PhysicalOutputChannel open_channel,
    PhysicalOutputChannel close_channel,
    const OutputRecord* output)
{
    if (output == nullptr || !output->commanded) {
        // No command since boot: STOP. Never infer CLOSE from the default false
        // value because that would move a valve during unrelated synchronization.
        bool ok = write_logical(open_channel, false);
        ok = write_logical(close_channel, false) && ok;
        return ok;
    }

    // Break-before-make in the core runtime first; the MCP backend additionally
    // clears the opposite bit in the same OLAT byte. The two layers make it
    // impossible for normal firmware sequencing to request OPEN+CLOSE together.
    if (output->active) {
        if (!write_logical(close_channel, false)) return false;
        return write_logical(open_channel, true);
    }

    if (!write_logical(open_channel, false)) return false;
    return write_logical(close_channel, true);
}

bool PhysicalOutputRuntime::force_safe()
{
    if (backend_ == nullptr) return false;
    bool ok = true;
    for (const auto channel : kAllChannels) {
        ok = write_safe(channel) && ok;
    }
    state_.outputs_enabled = false;
    if (!ok) state_.status = PhysicalOutputStatus::BackendError;
    return ok;
}

bool PhysicalOutputRuntime::synchronize(
    const SystemModel& model,
    const BootReadinessReport& readiness)
{
    if (backend_ == nullptr) return false;
    if (!readiness.outputs_allowed()) {
        state_.status = PhysicalOutputStatus::FailClosed;
        return force_safe();
    }

    state_.outputs_enabled = true;
    state_.status = PhysicalOutputStatus::Ready;

    bool ok = true;
    const auto* siren = model.output(1);
    ok = write_logical(
             PhysicalOutputChannel::Siren,
             siren != nullptr && siren->active) && ok;

    ok = write_valve(
             PhysicalOutputChannel::ColdValveOpen,
             PhysicalOutputChannel::ColdValveClose,
             model.output(2)) && ok;
    ok = write_valve(
             PhysicalOutputChannel::HotValveOpen,
             PhysicalOutputChannel::HotValveClose,
             model.output(3)) && ok;

    if (!ok) force_safe();
    return ok;
}

const char* to_string(PhysicalOutputStatus status)
{
    switch (status) {
        case PhysicalOutputStatus::Ready: return "ready";
        case PhysicalOutputStatus::FailClosed: return "fail_closed";
        case PhysicalOutputStatus::InvalidHardware: return "invalid_hardware";
        case PhysicalOutputStatus::BackendError: return "backend_error";
    }
    return "unknown";
}

}  // namespace hg
