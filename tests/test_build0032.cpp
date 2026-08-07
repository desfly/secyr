#include "test_framework.hpp"
#include "homeguard/system_api.hpp"
#include "homeguard/system_model.hpp"
#include <string>

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
}
