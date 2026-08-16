#include "homeguard/system_model.hpp"

#include <algorithm>

namespace hg {

bool SystemEventBus::subscribe(SystemEventCallback callback, void* context) {
    if (callback == nullptr || subscriber_count_ >= subscribers_.size()) return false;
    subscribers_[subscriber_count_++] = Subscriber{callback, context};
    return true;
}

bool SystemEventBus::publish(SystemEvent event) {
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
    if (queue_size_ == 0U) return false;
    const SystemEvent event = queue_[queue_head_];
    queue_head_ = (queue_head_ + 1U) % queue_.size();
    --queue_size_;

    for (std::size_t i = 0; i < subscriber_count_; ++i) {
        subscribers_[i].callback(event, subscribers_[i].context);
    }
    return true;
}

std::size_t SystemEventBus::dispatch_all() {
    std::size_t count = 0;
    while (dispatch_one()) ++count;
    return count;
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
    if (zone_count_ >= zones_.size() || zone(id) != nullptr) return false;
    auto& item = zones_[zone_count_++];
    item.id = id;
    copy_name(item.name, name);
    item.type = type;
    item.always_on = always_on;
    return true;
}

bool SystemModel::add_sensor(std::uint16_t id, ModelSensorType type) {
    if (sensor_count_ >= sensors_.size() || sensor(id) != nullptr) return false;
    auto& item = sensors_[sensor_count_++];
    item.id = id;
    item.type = type;
    return true;
}

bool SystemModel::add_output(std::uint16_t id, ModelOutputType type) {
    if (output_count_ >= outputs_.size() || output(id) != nullptr) return false;
    auto& item = outputs_[output_count_++];
    item.id = id;
    item.type = type;
    return true;
}

bool SystemModel::add_partition(std::uint16_t id) {
    if (partition_count_ >= partitions_.size() || partition(id) != nullptr) return false;
    partitions_[partition_count_++].id = id;
    return true;
}

bool SystemModel::set_zone_state(std::uint16_t id, ModelZoneState state, std::uint64_t now_ms) {
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
    for (std::size_t i = 0; i < output_count_; ++i) {
        auto& item = outputs_[i];
        if (item.id != id) continue;
        const bool changed = item.active != active;
        item.active = active;
        item.commanded = true;
        if (!changed) return true;
        return emit(active ? SystemEventType::OutputOn : SystemEventType::OutputOff, id, now_ms);
    }
    return false;
}

bool SystemModel::set_partition_arm(std::uint16_t id, PartitionArmState state, std::uint64_t now_ms) {
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
