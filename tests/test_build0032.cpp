#include "test_framework.hpp"
#include "homeguard/system_api.hpp"
#include "homeguard/system_model.hpp"
#include <string>

namespace {
struct PartitionEvents {
    hg::SystemEvent last{};
    unsigned count{};
};
void capture_partition_event(const hg::SystemEvent& event, void* context) {
    auto& captured = *static_cast<PartitionEvents*>(context);
    captured.last = event;
    ++captured.count;
}
}

void test_build0032() {
    hg::SystemEventBus bus;
    hg::SystemModel model(bus);

    CHECK(model.add_zone(1, "Front Door", hg::ModelZoneType::Perimeter));
    CHECK(model.add_zone(2, "Leak", hg::ModelZoneType::Flood, true));
    CHECK(model.add_sensor(1, hg::ModelSensorType::Digital));
    CHECK(model.add_output(1, hg::ModelOutputType::Valve));
    CHECK(model.add_partition(1));

    CHECK(model.set_zone_state(1, hg::ModelZoneState::Open, 100));
    CHECK(model.set_output_active(1, true, 200));
    CHECK(model.set_partition_arm(1, hg::PartitionArmState::Away, 300));

    const std::string status = hg::system_status_json(model, bus);
    CHECK(status.find("\"apiVersion\":1") != std::string::npos);
    CHECK(status.find("\"zones\":2") != std::string::npos);
    CHECK(status.find("\"published\":3") != std::string::npos);

    const std::string zones = hg::system_zones_json(model);
    CHECK(zones.find("Front Door") != std::string::npos);
    CHECK(zones.find("\"state\":\"open\"") != std::string::npos);
    CHECK(zones.find("\"alwaysOn\":true") != std::string::npos);

    const std::string outputs = hg::system_outputs_json(model);
    CHECK(outputs.find("\"type\":\"valve\"") != std::string::npos);
    CHECK(outputs.find("\"active\":true") != std::string::npos);

    const std::string partitions = hg::system_partitions_json(model);
    CHECK(partitions.find("\"armState\":\"away\"") != std::string::npos);

    hg::SystemEvent event{hg::SystemEventType::Alarm, 7, 1234, 55, 9};
    const std::string event_json = hg::system_event_json(event);
    CHECK(event_json.find("\"event\":\"alarm\"") != std::string::npos);
    CHECK(event_json.find("\"sequence\":55") != std::string::npos);
    // A partition alarm must reach MQTT/Android as critical ALARM, not ARMED.
    hg::SystemEventBus alarm_bus;
    hg::SystemModel alarm_model(alarm_bus);
    PartitionEvents captured;
    CHECK(alarm_bus.subscribe(capture_partition_event, &captured));
    CHECK(alarm_model.add_partition(1));
    CHECK(alarm_model.set_partition_arm(1, hg::PartitionArmState::Away, 100));
    CHECK(alarm_bus.dispatch_all() == 1);
    CHECK(captured.last.type == hg::SystemEventType::Armed);
    CHECK(alarm_model.set_partition_arm(1, hg::PartitionArmState::Alarm, 200));
    CHECK(alarm_bus.dispatch_all() == 1);
    CHECK(captured.last.type == hg::SystemEventType::Alarm);
    CHECK(captured.last.source_id == 1);
    CHECK(captured.last.value == static_cast<int>(hg::PartitionArmState::Alarm));
    CHECK(alarm_model.set_partition_arm(1, hg::PartitionArmState::Alarm, 201));
    CHECK(alarm_bus.dispatch_all() == 0);
    CHECK(alarm_model.set_partition_arm(1, hg::PartitionArmState::Disarmed, 300));
    CHECK(alarm_bus.dispatch_all() == 1);
    CHECK(captured.last.type == hg::SystemEventType::Disarmed);
    CHECK(captured.count == 3);

}
