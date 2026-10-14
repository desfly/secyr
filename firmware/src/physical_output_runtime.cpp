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

    // LIGHT/LOCK use virtual output IDs routed to MCP23017 GPA0/GPA1.
    // ESP32 GPIO1 and GPIO2 remain unassigned.
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
        bool safe = true;
        safe = write_safe(hardware.pins.siren) && safe;
        safe = write_safe(hardware.pins.valve1) && safe;
        safe = write_safe(hardware.pins.valve2) && safe;
        state_.status = safe ? PhysicalOutputStatus::FailClosed : PhysicalOutputStatus::BackendError;
        return safe;
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

    // Light/lock outputs follow the model through MCP23017 GPA0/GPA1.
    // GPIO1 and GPIO2 must remain free.
    bool ok = true;
    ok = write_logical(direct_light_relay_gpio, output_state(model, 4)) && ok;
    ok = write_logical(direct_lock_relay_gpio, output_state(model, 5)) && ok;
    if (!ok) {
        force_safe();
        return false;
    }

    // The remaining physical outputs stay fail-closed until commissioning is
    // valid. Keep the MCP23017 light/lock outputs independently controlled.
    if (!readiness.outputs_allowed() || !hardware_verification_allows_outputs(*hardware_)) {
        ok = write_safe(hardware_->pins.siren) && ok;
        ok = write_safe(hardware_->pins.valve1) && ok;
        ok = write_safe(hardware_->pins.valve2) && ok;
        state_.outputs_enabled = false;
        state_.status = !ok
            ? PhysicalOutputStatus::BackendError
            : (hardware_verification_allows_outputs(*hardware_)
                ? PhysicalOutputStatus::FailClosed
                : PhysicalOutputStatus::InvalidHardware);
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
