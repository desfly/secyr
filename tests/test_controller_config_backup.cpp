#include "homeguard/controller_config_backup.hpp"
#include "test_framework.hpp"

#include <string>

void test_controller_config_backup() {
    hg::ControllerConfig source{};
    source.entry_delay_ms = 45000U;
    source.exit_delay_ms = 55000U;
    source.network_debounce_ms = 1500U;
    source.failover_hold_ms = 7500U;
    source.request_ttl_ms = 180000U;
    source.zones[0].enabled = false;
    source.zones[0].normally_closed = false;
    source.zones[0].debounce_ms = 125U;
    source.pressures[0].enabled = true;
    source.pressures[0].low = 1.5F;
    source.pressures[0].high = 5.25F;
    source.pressures[0].hysteresis = 0.2F;

    const auto json = hg::ControllerConfigBackup::encode(source);
    CHECK(json.find("homeguard-s3-controller-settings") != std::string::npos);
    CHECK(json.find("password") == std::string::npos);
    CHECK(json.find("token") == std::string::npos);
    CHECK(json.find("credential") == std::string::npos);

    hg::ControllerConfig restored{};
    std::string error;
    CHECK(hg::ControllerConfigBackup::decode(json, restored, error));
    CHECK(error.empty());
    CHECK(restored.entry_delay_ms == source.entry_delay_ms);
    CHECK(restored.exit_delay_ms == source.exit_delay_ms);
    CHECK(restored.zones[0].enabled == source.zones[0].enabled);
    CHECK(restored.zones[0].normally_closed == source.zones[0].normally_closed);
    CHECK(restored.zones[0].debounce_ms == source.zones[0].debounce_ms);
    CHECK(restored.pressures[0].low == source.pressures[0].low);
    CHECK(restored.pressures[0].high == source.pressures[0].high);

    auto wrong_version = json;
    const auto version_pos = wrong_version.find("\"version\":1");
    CHECK(version_pos != std::string::npos);
    wrong_version.replace(version_pos, 11U, "\"version\":2");
    CHECK(!hg::ControllerConfigBackup::decode(wrong_version, restored, error));
    CHECK(error == "unsupported_version");

    auto unsafe = json;
    const auto debounce_pos = unsafe.find("\"zone0DebounceMs\":125");
    CHECK(debounce_pos != std::string::npos);
    unsafe.replace(debounce_pos, 21U, "\"zone0DebounceMs\":99999");
    CHECK(!hg::ControllerConfigBackup::decode(unsafe, restored, error));
    CHECK(error == "zone_debounce_out_of_range");
}
