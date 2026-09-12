#include "homeguard/physical_output_runtime.hpp"

namespace hg {
namespace {

bool output_state(const SystemModel& model, std::uint16_t id) {
    const auto* output = model.output(id);
    return output != nullptr && output->active;
}

}  // namespace

bool PhysicalOutputRuntime::initialize(
    PhysicalOutputBackend& backend,
    const HardwareVerificationRecord& hardware,
    const BootReadinessReport& readiness)
{
    backend_ = &backend;
    hardware_ = &hardware;
    state_ = {};

    // LIGHT and LOCK are the current physical bench relays and are wired
    // directly to ESP32-S3 GPIO1/GPIO2. Configure them first and keep them
    // independent from the future MCP23017 commissioning gate.
    const int direct_gpios[] = {direct_light_relay_gpio, direct_lock_relay_gpio};
    for (const int gpio : direct_gpios) {
        if (!backend_->configure_output(gpio, false)) {
            ++state_.failures;
            state_.status = PhysicalOutputStatus::BackendError;
            return false;
        }
    }

    // Siren/valves remain protected by the verified commissioning record.
    if (!hardware_verification_allows_outputs(hardware)) {
        state_.status = PhysicalOutputStatus::InvalidHardware;
        return true;
    }

    const int commissioned_gpios[] = {hardware.pins.siren, hardware.pins.valve1, hardware.pins.valve2};
    for (const int gpio : commissioned_gpios) {
        if (gpio == gpio_unassigned) continue;
        if (!backend_->configure_output(gpio, false)) {
            ++state_.failures;
            state_.status = PhysicalOutputStatus::BackendError;
            force_safe();
            return false;
        }
    }

    if (!readiness.outputs_allowed()) {
        write_safe(hardware.pins.siren);
        write_safe(hardware.pins.valve1);
        write_safe(hardware.pins.valve2);
        state_.status = PhysicalOutputStatus::FailClosed;
        return true;
    }

    state_.outputs_enabled = true;
    state_.status = PhysicalOutputStatus::Ready;
    return true;
}

bool PhysicalOutputRuntime::write_safe(int gpio) {
    if (gpio == gpio_unassigned) return true;
    if (backend_ == nullptr || !backend_->write_output(gpio, false)) {
        ++state_.failures;
        return false;
    }
    ++state_.writes;
    return true;
}

bool PhysicalOutputRuntime::write_logical(int gpio, bool active) {
    if (gpio == gpio_unassigned) return true;
    if (backend_ == nullptr || !backend_->write_output(gpio, active)) {
        ++state_.failures;
        state_.status = PhysicalOutputStatus::BackendError;
        state_.outputs_enabled = false;
        return false;
    }
    ++state_.writes;
    return true;
}

bool PhysicalOutputRuntime::force_safe() {
    if (hardware_ == nullptr) return false;
    bool ok = true;
    ok = write_safe(hardware_->pins.siren) && ok;
    ok = write_safe(hardware_->pins.valve1) && ok;
    ok = write_safe(hardware_->pins.valve2) && ok;
    ok = write_safe(direct_light_relay_gpio) && ok;
    ok = write_safe(direct_lock_relay_gpio) && ok;
    state_.outputs_enabled = false;
    if (!ok) state_.status = PhysicalOutputStatus::BackendError;
    return ok;
}

bool PhysicalOutputRuntime::synchronize(const SystemModel& model, const BootReadinessReport& readiness) {
    if (backend_ == nullptr || hardware_ == nullptr) return false;

    // Direct relays must follow the model regardless of commissioning state.
    // This is the current tested wiring: output 4 -> GPIO1, output 5 -> GPIO2.
    bool ok = true;
    ok = write_logical(direct_light_relay_gpio, output_state(model, 4)) && ok;
    ok = write_logical(direct_lock_relay_gpio, output_state(model, 5)) && ok;
    if (!ok) {
        force_safe();
        return false;
    }

    // The remaining physical outputs stay fail-closed until commissioning is
    // valid. Do not turn the already-installed GPIO1/GPIO2 relays off here.
    if (!readiness.outputs_allowed() || !hardware_verification_allows_outputs(*hardware_)) {
        ok = write_safe(hardware_->pins.siren) && ok;
        ok = write_safe(hardware_->pins.valve1) && ok;
        ok = write_safe(hardware_->pins.valve2) && ok;
        state_.outputs_enabled = false;
        state_.status = hardware_verification_allows_outputs(*hardware_)
            ? PhysicalOutputStatus::FailClosed
            : PhysicalOutputStatus::InvalidHardware;
        return ok;
    }

    state_.outputs_enabled = true;
    state_.status = PhysicalOutputStatus::Ready;
    ok = write_logical(hardware_->pins.siren, output_state(model, 1)) && ok;
    ok = write_logical(hardware_->pins.valve1, output_state(model, 2)) && ok;
    ok = write_logical(hardware_->pins.valve2, output_state(model, 3)) && ok;
    if (!ok) force_safe();
    return ok;
}

const char* to_string(PhysicalOutputStatus status) {
    switch (status) {
    case PhysicalOutputStatus::Ready: return "ready";
    case PhysicalOutputStatus::FailClosed: return "fail_closed";
    case PhysicalOutputStatus::InvalidHardware: return "invalid_hardware";
    case PhysicalOutputStatus::BackendError: return "backend_error";
    }
    return "unknown";
}

}  // namespace hg
