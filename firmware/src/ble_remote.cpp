#include "homeguard/ble_remote.hpp"

#include <algorithm>

namespace hg {

void BleRemoteRegistry::copy_name(std::array<char, 24>& out, std::string_view value)
{
    out.fill('\0');
    const auto count = std::min(value.size(), out.size() - 1U);
    std::copy_n(value.data(), count, out.data());
}

BleRemoteBinding* BleRemoteRegistry::find(const BleRemoteIdentity& identity)
{
    for (std::size_t i = 0; i < size_; ++i) {
        if (bindings_[i].identity == identity) return &bindings_[i];
    }
    return nullptr;
}

const BleRemoteBinding* BleRemoteRegistry::find(const BleRemoteIdentity& identity) const
{
    for (std::size_t i = 0; i < size_; ++i) {
        if (bindings_[i].identity == identity) return &bindings_[i];
    }
    return nullptr;
}

bool BleRemoteRegistry::bind(
    const BleRemoteIdentity& identity,
    BleRemoteProfile profile,
    std::string_view name,
    std::uint32_t permissions,
    bool replay_counter_required)
{
    if (permissions == 0U || name.empty()) return false;
    if (auto* existing = find(identity)) {
        existing->profile = profile;
        existing->permissions = permissions;
        existing->enabled = true;
        existing->replay_counter_required = replay_counter_required;
        existing->counter_initialized = false;
        existing->last_counter = 0;
        copy_name(existing->name, name);
        return true;
    }
    if (size_ >= capacity) return false;
    auto& binding = bindings_[size_++];
    binding = {};
    binding.identity = identity;
    binding.profile = profile;
    binding.permissions = permissions;
    binding.enabled = true;
    binding.replay_counter_required = replay_counter_required;
    copy_name(binding.name, name);
    return true;
}

bool BleRemoteRegistry::unbind(const BleRemoteIdentity& identity)
{
    for (std::size_t i = 0; i < size_; ++i) {
        if (!(bindings_[i].identity == identity)) continue;
        for (std::size_t j = i + 1; j < size_; ++j) bindings_[j - 1] = bindings_[j];
        bindings_[--size_] = {};
        return true;
    }
    return false;
}

bool BleRemoteRegistry::set_enabled(const BleRemoteIdentity& identity, bool enabled)
{
    auto* binding = find(identity);
    if (!binding) return false;
    binding->enabled = enabled;
    return true;
}

const char* BleRemoteRegistry::command_for(BleRemoteAction action)
{
    switch (action) {
        case BleRemoteAction::ArmAway: return "security.arm_away";
        case BleRemoteAction::ArmHome: return "security.arm_home";
        case BleRemoteAction::Disarm: return "security.disarm";
        case BleRemoteAction::LockPulse: return "lock.pulse";
        case BleRemoteAction::LightToggle: return "light.toggle";
        case BleRemoteAction::Panic: return "security.panic";
    }
    return nullptr;
}

BleRemoteResult BleRemoteRegistry::accept(const BleRemoteEvent& event)
{
    auto* binding = find(event.identity);
    if (!binding) return {BleRemoteDecision::UnknownRemote, nullptr, nullptr};
    if (!binding->enabled) return {BleRemoteDecision::Disabled, binding, nullptr};

    const auto* command = command_for(event.action);
    if (!command) return {BleRemoteDecision::InvalidAction, binding, nullptr};
    if ((binding->permissions & permission(event.action)) == 0U) {
        return {BleRemoteDecision::PermissionDenied, binding, command};
    }

    if (binding->replay_counter_required) {
        if (!event.counter_valid) return {BleRemoteDecision::ReplayRejected, binding, command};
        if (binding->counter_initialized && event.counter <= binding->last_counter) {
            return {BleRemoteDecision::ReplayRejected, binding, command};
        }
        binding->counter_initialized = true;
        binding->last_counter = event.counter;
    } else if (event.counter_valid) {
        if (binding->counter_initialized && event.counter == binding->last_counter) {
            return {BleRemoteDecision::ReplayRejected, binding, command};
        }
        binding->counter_initialized = true;
        binding->last_counter = event.counter;
    }

    return {BleRemoteDecision::Accepted, binding, command};
}

const char* to_string(BleRemoteDecision decision) noexcept
{
    switch (decision) {
        case BleRemoteDecision::Accepted: return "accepted";
        case BleRemoteDecision::UnknownRemote: return "unknown_remote";
        case BleRemoteDecision::Disabled: return "disabled";
        case BleRemoteDecision::PermissionDenied: return "permission_denied";
        case BleRemoteDecision::ReplayRejected: return "replay_rejected";
        case BleRemoteDecision::InvalidAction: return "invalid_action";
    }
    return "invalid_action";
}

}  // namespace hg
