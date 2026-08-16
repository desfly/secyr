#include "homeguard/system_model.hpp"

#include <algorithm>

namespace hg {

bool SystemEventBus::subscribe(SystemEventCallback callback, void* context) {
    if (callback == nullptr) return false;
    std::scoped_lock lock(mutex_);
    if (subscriber_count_ >= subscribers_.size()) return false;
    subscribers_[subscriber_count_++] = Subscriber{callback, context};
    return true;
}

bool SystemEventBus::publish(SystemEvent event) {
    std::scoped_lock lock(mutex_);
    event.sequence = next_sequence_++;
    ++published_;

    if (queue_size_ == queue_.size()) {
        queue_head_ = (queue_head_ + 1U) % queue_.size();
        --queue_size_;
        ++dropped_;
    }

    queue_[queue_tail_] = event;
    queue_tail_ = (queue_tail_ + 1U) % queue_.size();
    ++queue_size_;
    return true;
}

bool SystemEventBus::dispatch_one() {
    SystemEvent event{};
    std::array<Subscriber, subscriber_capacity> subscribers{};
    std::size_t subscriber_count = 0;
    {
        std::scoped_lock lock(mutex_);
        if (queue_size_ == 0U) return false;
        event = queue_[queue_head_];
        queue_head_ = (queue_head_ + 1U) % queue_.size();
        --queue_size_;
        subscriber_count = subscriber_count_;
        std::copy_n(subscribers_.begin(), subscriber_count, subscribers.begin());
    }

    // Never call arbitrary subscribers while the queue mutex is held. A
    // callback may read/update the model or publish another event.
    for (std::size_t i = 0; i < subscriber_count; ++i) {
        subscribers[i].callback(event, subscribers[i].context);
    }
    return true;
}

std::size_t SystemEventBus::dispatch_all() {
    std::size_t count = 0;
    while (dispatch_one()) ++count;
    return count;
}

std::size_t SystemEventBus::queued() const {
    std::scoped_lock lock(mutex_);
    return queue_size_;
}

std::uint64_t SystemEventBus::published() const {
    std::scoped_lock lock(mutex_);
    return published_;
}

std::uint64_t SystemEventBus::dropped() const {
    std::scoped_lock lock(mutex_);
    return dropped_;
}

void SystemModel::copy_name(std::array<char, 24>& destination, std::string_view source) {
    destination.fill('\0');
    const auto count = std::min(source.size(), destination.size() - 1U);
    std::copy_n(source.begin(), count, destination.begin());
}

bool SystemModel::emit(SystemEventType type, std::uint16_t source_id, std::uint64_t now_ms, std::int32_t value) {
    return bus_.publish(SystemEvent{type, source_id, now_ms, 0, value});
}

bool SystemModel::add_zone(std::uint16_t id, std::string_view name, ModelZoneType type, bool always_on) {
    std::scoped_lock lock(mutex_);
    if (zone_count_ >= zones_.size()) return false;
    for (std::size_t i = 0; i < zone_count_; ++i) if (zones_[i].id == id) return false;
    auto& item = zones_[zone_count_++];
    item.id = id;
    copy_name(item.name, name);
    item.type = type;
    item.always_on = always_on;
    return true;
}

bool SystemModel::add_sensor(std::uint16_t id, ModelSensorType type) {
    std::scoped_lock lock(mutex_);
    if (sensor_count_ >= sensors_.size()) return false;
    for (std::size_t i = 0; i < sensor_count_; ++i) if (sensors_[i].id == id) return false;
    auto& item = sensors_[sensor_count_++];
    item.id = id;
    item.type = type;
    return true;
}

bool SystemModel::add_output(std::uint16_t id, ModelOutputType type) {
    std::scoped_lock lock(mutex_);
    if (output_count_ >= outputs_.size()) return false;
    for (std::size_t i = 0; i < output_count_; ++i) if (outputs_[i].id == id) return false;
    auto& item = outputs_[output_count_++];
    item.id = id;
    item.type = type;
    return true;
}

bool SystemModel::add_partition(std::uint16_t id) {
    std::scoped_lock lock(mutex_);
    if (partition_count_ >= partitions_.size()) return false;
    for (std::size_t i = 0; i < partition_count_; ++i) if (partitions_[i].id == id) return false;
    partitions_[partition_count_++].id = id;
    return true;
}

bool SystemModel::set_zone_state(std::uint16_t id, ModelZoneState state, std::uint64_t now_ms) {
    std::scoped_lock lock(mutex_);
    for (std::size_t i = 0; i < zone_count_; ++i) {
        auto& item = zones_[i];
        if (item.id != id) continue;
        if (item.state == state) return true;
        item.state = state;
        switch (state) {
            case ModelZoneState::Normal: return emit(SystemEventType::ZoneClosed, id, now_ms);
            case ModelZoneState::Open: return emit(SystemEventType::ZoneOpen, id, now_ms);
            case ModelZoneState::Alarm: return emit(SystemEventType::Alarm, id, now_ms);
            case ModelZoneState::Tamper: return emit(SystemEventType::Tamper, id, now_ms);
            case ModelZoneState::Fault:
            case ModelZoneState::Bypassed:
                return emit(SystemEventType::ConfigChanged, id, now_ms, static_cast<std::int32_t>(state));
        }
    }
    return false;
}

bool SystemModel::set_output_active(std::uint16_t id, bool active, std::uint64_t now_ms) {
    std::scoped_lock lock(mutex_);
    for (std::size_t i = 0; i < output_count_; ++i) {
        auto& item = outputs_[i];
        if (item.id != id) continue;
        const bool changed = item.active != active;
        item.active = active;
        item.commanded = true;
        ++item.command_revision;
        // A repeated OPEN/CLOSE is still a new physical command revision even
        // when the logical active flag does not change. No duplicate event is
        // needed, but actuator runtime will see and consume the new revision.
        if (!changed) return true;
        return emit(active ? SystemEventType::OutputOn : SystemEventType::OutputOff, id, now_ms);
    }
    return false;
}

bool SystemModel::set_partition_arm(std::uint16_t id, PartitionArmState state, std::uint64_t now_ms) {
    std::scoped_lock lock(mutex_);
    for (std::size_t i = 0; i < partition_count_; ++i) {
        auto& item = partitions_[i];
        if (item.id != id) continue;
        if (item.arm_state == state) return true;
        item.arm_state = state;
        const bool disarmed = state == PartitionArmState::Disarmed;
        return emit(disarmed ? SystemEventType::Disarmed : SystemEventType::Armed, id, now_ms, static_cast<std::int32_t>(state));
    }
    return false;
}

bool SystemModel::zone_snapshot(std::uint16_t id, ZoneRecord& out) const {
    std::scoped_lock lock(mutex_);
    for (std::size_t i = 0; i < zone_count_; ++i) {
        if (zones_[i].id == id) { out = zones_[i]; return true; }
    }
    return false;
}

bool SystemModel::sensor_snapshot(std::uint16_t id, SensorRecord& out) const {
    std::scoped_lock lock(mutex_);
    for (std::size_t i = 0; i < sensor_count_; ++i) {
        if (sensors_[i].id == id) { out = sensors_[i]; return true; }
    }
    return false;
}

bool SystemModel::output_snapshot(std::uint16_t id, OutputRecord& out) const {
    std::scoped_lock lock(mutex_);
    for (std::size_t i = 0; i < output_count_; ++i) {
        if (outputs_[i].id == id) { out = outputs_[i]; return true; }
    }
    return false;
}

bool SystemModel::partition_snapshot(std::uint16_t id, PartitionRecord& out) const {
    std::scoped_lock lock(mutex_);
    for (std::size_t i = 0; i < partition_count_; ++i) {
        if (partitions_[i].id == id) { out = partitions_[i]; return true; }
    }
    return false;
}

bool SystemModel::zone_at_snapshot(std::size_t index, ZoneRecord& out) const {
    std::scoped_lock lock(mutex_);
    if (index >= zone_count_) return false;
    out = zones_[index];
    return true;
}

bool SystemModel::sensor_at_snapshot(std::size_t index, SensorRecord& out) const {
    std::scoped_lock lock(mutex_);
    if (index >= sensor_count_) return false;
    out = sensors_[index];
    return true;
}

bool SystemModel::output_at_snapshot(std::size_t index, OutputRecord& out) const {
    std::scoped_lock lock(mutex_);
    if (index >= output_count_) return false;
    out = outputs_[index];
    return true;
}

bool SystemModel::partition_at_snapshot(std::size_t index, PartitionRecord& out) const {
    std::scoped_lock lock(mutex_);
    if (index >= partition_count_) return false;
    out = partitions_[index];
    return true;
}

std::size_t SystemModel::zone_count() const {
    std::scoped_lock lock(mutex_);
    return zone_count_;
}

std::size_t SystemModel::sensor_count() const {
    std::scoped_lock lock(mutex_);
    return sensor_count_;
}

std::size_t SystemModel::output_count() const {
    std::scoped_lock lock(mutex_);
    return output_count_;
}

std::size_t SystemModel::partition_count() const {
    std::scoped_lock lock(mutex_);
    return partition_count_;
}

const ZoneRecord* SystemModel::zone(std::uint16_t id) const {
    for (std::size_t i = 0; i < zone_count_; ++i) if (zones_[i].id == id) return &zones_[i];
    return nullptr;
}

const SensorRecord* SystemModel::sensor(std::uint16_t id) const {
    for (std::size_t i = 0; i < sensor_count_; ++i) if (sensors_[i].id == id) return &sensors_[i];
    return nullptr;
}

const OutputRecord* SystemModel::output(std::uint16_t id) const {
    for (std::size_t i = 0; i < output_count_; ++i) if (outputs_[i].id == id) return &outputs_[i];
    return nullptr;
}

const PartitionRecord* SystemModel::partition(std::uint16_t id) const {
    for (std::size_t i = 0; i < partition_count_; ++i) if (partitions_[i].id == id) return &partitions_[i];
    return nullptr;
}

}  // namespace hg
