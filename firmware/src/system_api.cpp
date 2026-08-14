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
std::string_view output_name(ModelOutputType type, std::size_t index) {
    switch (type) {
        case ModelOutputType::Siren: return "Siren";
        case ModelOutputType::Valve: return index == 1U ? "Valve 1" : "Valve 2";
        case ModelOutputType::Light: return "Light";
        default: return "Relay";
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
    for (std::size_t i = 0; i < model.zone_count(); ++i) {
        if (i != 0U) out << ',';
        const auto* z = model.zone_at(i);
        out << "{\"index\":" << i
            << ",\"id\":" << z->id
            << ",\"name\":\"" << z->name.data()
            << "\",\"state\":\"" << zone_state_name(z->state)
            << "\",\"enabled\":" << (z->enabled ? "true" : "false")
            << ",\"bypassed\":" << (z->bypassed ? "true" : "false")
            << ",\"alwaysOn\":" << (z->always_on ? "true" : "false") << '}';
    }
    out << "]}";
    return out.str();
}

std::string system_outputs_json(const SystemModel& model) {
    std::ostringstream out;
    out << "{\"outputs\":[";
    for (std::size_t i = 0; i < model.output_count(); ++i) {
        if (i != 0U) out << ',';
        const auto* item = model.output_at(i);
        out << "{\"index\":" << i
            << ",\"id\":" << item->id
            << ",\"name\":\"" << output_name(item->type, i)
            << "\",\"type\":\"" << output_type_name(item->type)
            << "\",\"active\":" << (item->active ? "true" : "false")
            << ",\"state\":\"" << (item->active ? "on" : "off")
            << "\",\"timeoutMs\":" << item->timeout_ms << '}';
    }
    out << "]}";
    return out.str();
}

std::string system_partitions_json(const SystemModel& model) {
    std::ostringstream out;
    out << "{\"partitions\":[";
    for (std::size_t i = 0; i < model.partition_count(); ++i) {
        if (i != 0U) out << ',';
        const auto* item = model.partition_at(i);
        out << "{\"id\":" << item->id << ",\"armState\":\"" << arm_state_name(item->arm_state)
            << "\",\"zoneCount\":" << item->zone_count << '}';
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
