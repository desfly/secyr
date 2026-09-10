#include "homeguard/ble_remote.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace hg {

bool BleRemotePermissions::allows(BleRemoteAction action) const
{
    switch (action) {
        case BleRemoteAction::ArmHome: return arm_home;
        case BleRemoteAction::ArmAway: return arm_away;
        case BleRemoteAction::Disarm: return disarm;
        case BleRemoteAction::Light: return light;
        case BleRemoteAction::LockPulse: return lock_pulse;
        case BleRemoteAction::Panic: return panic;
    }
    return false;
}

bool BleRemoteRegistry::valid_text(std::string_view value, std::size_t capacity)
{
    return !value.empty() && value.size() < capacity;
}

void BleRemoteRegistry::copy_text(char* destination, std::size_t capacity, std::string_view source)
{
    std::fill(destination, destination + capacity, '\0');
    const auto size = std::min(source.size(), capacity - 1U);
    std::copy_n(source.begin(), size, destination);
}

BleRemoteBinding* BleRemoteRegistry::find(const std::array<std::uint8_t, 16>& identity)
{
    for (auto& binding : bindings_) {
        if (binding.occupied && binding.identity == identity) return &binding;
    }
    return nullptr;
}

const BleRemoteBinding* BleRemoteRegistry::find(const std::array<std::uint8_t, 16>& identity) const
{
    for (const auto& binding : bindings_) {
        if (binding.occupied && binding.identity == identity) return &binding;
    }
    return nullptr;
}

bool BleRemoteRegistry::bind(const std::array<std::uint8_t, 16>& identity,
                             std::string_view owner_user_id,
                             std::string_view name,
                             BleRemotePermissions permissions)
{
    const bool nonzero = std::any_of(identity.begin(), identity.end(), [](std::uint8_t value) { return value != 0; });
    if (!nonzero || !valid_text(owner_user_id, BleRemoteBinding{}.owner_user_id.size()) ||
        !valid_text(name, BleRemoteBinding{}.name.size())) return false;

    if (auto* existing = find(identity); existing != nullptr) {
        copy_text(existing->owner_user_id.data(), existing->owner_user_id.size(), owner_user_id);
        copy_text(existing->name.data(), existing->name.size(), name);
        existing->permissions = permissions;
        return true;
    }

    for (auto& binding : bindings_) {
        if (!binding.occupied) {
            binding = {};
            binding.identity = identity;
            copy_text(binding.owner_user_id.data(), binding.owner_user_id.size(), owner_user_id);
            copy_text(binding.name.data(), binding.name.size(), name);
            binding.permissions = permissions;
            binding.occupied = true;
            return true;
        }
    }
    return false;
}

bool BleRemoteRegistry::unbind(const std::array<std::uint8_t, 16>& identity)
{
    auto* binding = find(identity);
    if (binding == nullptr) return false;
    *binding = {};
    return true;
}

BleRemoteResult BleRemoteRegistry::authorize(const BleRemoteEvent& event)
{
    const bool nonzero = std::any_of(event.identity.begin(), event.identity.end(), [](std::uint8_t value) { return value != 0; });
    if (!nonzero) return {BleRemoteDecision::InvalidEvent, event.action, 0};

    auto* binding = find(event.identity);
    if (binding == nullptr) return {BleRemoteDecision::UnknownRemote, event.action, 0};
    const auto index = static_cast<std::size_t>(binding - bindings_.data());

    if (!binding->permissions.allows(event.action)) {
        return {BleRemoteDecision::ActionDenied, event.action, index};
    }

    if (event.has_counter) {
        if (binding->counter_initialized && event.counter <= binding->last_counter) {
            return {BleRemoteDecision::Replay, event.action, index};
        }
        binding->last_counter = event.counter;
        binding->counter_initialized = true;
    }

    return {BleRemoteDecision::Accepted, event.action, index};
}

std::size_t BleRemoteRegistry::count() const
{
    return static_cast<std::size_t>(std::count_if(bindings_.begin(), bindings_.end(), [](const BleRemoteBinding& binding) {
        return binding.occupied;
    }));
}

const BleRemoteBinding* BleRemoteRegistry::binding_at(std::size_t index) const
{
    return index < bindings_.size() && bindings_[index].occupied ? &bindings_[index] : nullptr;
}

const BleRemoteBinding* BleRemoteRegistry::binding_for(const std::array<std::uint8_t, 16>& identity) const
{
    return find(identity);
}

bool BleRemoteRegistry::import_binding(const BleRemoteBinding& binding)
{
    if (!binding.occupied) return true;
    if (binding.owner_user_id.back() != '\0' || binding.name.back() != '\0') return false;
    if (!valid_text(binding.owner_user_id.data(), binding.owner_user_id.size()) ||
        !valid_text(binding.name.data(), binding.name.size())) return false;
    if (!bind(binding.identity, binding.owner_user_id.data(), binding.name.data(), binding.permissions)) return false;
    auto* imported = find(binding.identity);
    if (imported == nullptr) return false;
    imported->last_counter = binding.last_counter;
    imported->counter_initialized = binding.counter_initialized;
    return true;
}

void BleRemoteRegistry::clear()
{
    bindings_.fill({});
}

std::string_view ble_remote_action_name(BleRemoteAction action)
{
    switch (action) {
        case BleRemoteAction::ArmHome: return "arm_home";
        case BleRemoteAction::ArmAway: return "arm_away";
        case BleRemoteAction::Disarm: return "disarm";
        case BleRemoteAction::Light: return "light";
        case BleRemoteAction::LockPulse: return "lock_pulse";
        case BleRemoteAction::Panic: return "panic";
    }
    return "unknown";
}

std::string_view ble_remote_decision_name(BleRemoteDecision decision)
{
    switch (decision) {
        case BleRemoteDecision::Accepted: return "accepted";
        case BleRemoteDecision::UnknownRemote: return "unknown_remote";
        case BleRemoteDecision::ActionDenied: return "action_denied";
        case BleRemoteDecision::Replay: return "replay";
        case BleRemoteDecision::InvalidEvent: return "invalid_event";
    }
    return "invalid_event";
}

}  // namespace hg
