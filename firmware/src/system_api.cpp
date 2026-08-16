#include "homeguard/system_api.hpp"

#include <sstream>

namespace hg {
namespace {
std::string_view zone_state_name(ModelZoneState state) {
    switch (state) {
        case ModelZoneState::Open: return "open";
        case ModelZoneState::Alarm: return "alarm";
        case ModelZoneState::Fault: return "fault";
        case ModelZoneState::Tamper: return "tamper";
        case ModelZoneState::Bypassed: return "bypassed";
        default: return "normal";
    }
}
std::string_view output_type_name(ModelOutputType type) {
    switch (type) {
        case ModelOutputType::Siren: return "siren";
        case ModelOutputType::Valve: return "valve";
        case ModelOutputType::Light: return "light";
        default: return "relay";
    }
}
std::string_view arm_state_name(PartitionArmState state) {
    switch (state) {
        case PartitionArmState::Stay: return "stay";
        case PartitionArmState::Away: return "away";
        case PartitionArmState::Alarm: return "alarm";
        default: return "disarmed";
    }
}
}

std::string_view system_event_type_name(SystemEventType type) {
    switch (type) {
        case SystemEventType::ZoneOpen: return "zone.open";
        case SystemEventType::ZoneClosed: return "zone.closed";
        case SystemEventType::Alarm: return "alarm";
        case SystemEventType::Tamper: return "tamper";
        case SystemEventType::SensorOffline: return "sensor.offline";
        case SystemEventType::BatteryLow: return "battery.low";
        case SystemEventType::OutputOn: return "output.on";
        case SystemEventType::OutputOff: return "output.off";
        case SystemEventType::Armed: return "partition.armed";
        case SystemEventType::Disarmed: return "partition.disarmed";
        default: return "config.changed";
    }
}

std::string system_status_json(const SystemModel& model, const SystemEventBus& bus) {
    std::ostringstream out;
    out << "{\"apiVersion\":1,\"zones\":" << model.zone_count()
        << ",\"sensors\":" << model.sensor_count()
        << ",\"outputs\":" << model.output_count()
        << ",\"partitions\":" << model.partition_count()
        << ",\"eventBus\":{\"queued\":" << bus.queued()
        << ",\"published\":" << bus.published()
        << ",\"dropped\":" << bus.dropped() << "}}";
    return out.str();
}

std::string system_zones_json(const SystemModel& model) {
    std::ostringstream out;
    out << "{\"zones\":[";
    bool first = true;
    const auto count = model.zone_count();
    for (std::size_t i = 0; i < count; ++i) {
        ZoneRecord z{};
        if (!model.zone_at_snapshot(i, z)) continue;
        if (!first) out << ',';
        first = false;
        out << "{\"id\":" << z.id << ",\"name\":\"" << z.name.data()
            << "\",\"state\":\"" << zone_state_name(z.state)
            << "\",\"enabled\":" << (z.enabled ? "true" : "false")
            << ",\"bypassed\":" << (z.bypassed ? "true" : "false")
            << ",\"alwaysOn\":" << (z.always_on ? "true" : "false") << '}';
    }
    out << "]}";
    return out.str();
}

std::string system_outputs_json(const SystemModel& model) {
    std::ostringstream out;
    out << "{\"outputs\":[";
    bool first = true;
    const auto count = model.output_count();
    for (std::size_t i = 0; i < count; ++i) {
        OutputRecord item{};
        if (!model.output_at_snapshot(i, item)) continue;
        if (!first) out << ',';
        first = false;
        out << "{\"id\":" << item.id << ",\"type\":\"" << output_type_name(item.type)
            << "\",\"active\":" << (item.active ? "true" : "false")
            << ",\"timeoutMs\":" << item.timeout_ms << '}';
    }
    out << "]}";
    return out.str();
}

std::string system_partitions_json(const SystemModel& model) {
    std::ostringstream out;
    out << "{\"partitions\":[";
    bool first = true;
    const auto count = model.partition_count();
    for (std::size_t i = 0; i < count; ++i) {
        PartitionRecord item{};
        if (!model.partition_at_snapshot(i, item)) continue;
        if (!first) out << ',';
        first = false;
        out << "{\"id\":" << item.id << ",\"armState\":\"" << arm_state_name(item.arm_state)
            << "\",\"zoneCount\":" << item.zone_count << '}';
    }
    out << "]}";
    return out.str();
}

std::string system_event_json(const SystemEvent& event) {
    std::ostringstream out;
    out << "{\"event\":\"" << system_event_type_name(event.type)
        << "\",\"sourceId\":" << event.source_id
        << ",\"timestampMs\":" << event.timestamp_ms
        << ",\"sequence\":" << event.sequence
        << ",\"value\":" << event.value << '}';
    return out.str();
}

}  // namespace hg
