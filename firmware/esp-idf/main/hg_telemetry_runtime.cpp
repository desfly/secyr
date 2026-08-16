#include "hg_telemetry_runtime.hpp"
#include "hg_hardware_bootstrap.hpp"
#include "websocket_telemetry.hpp"
#include "homeguard/system_model.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ctime>

namespace homeguard::idf {

namespace {

constexpr const char* kTag = "hg_telemetry";
constexpr TickType_t kTelemetryPeriod = pdMS_TO_TICKS(1000);
constexpr std::uint32_t kOneWireRediscoveryCycles = 60U;

hg::HealthState module_health(homeguard::HardwareModuleState state)
{
    switch (state) {
        case homeguard::HardwareModuleState::Ready:
            return hg::HealthState::Ok;
        case homeguard::HardwareModuleState::Degraded:
        case homeguard::HardwareModuleState::Missing:
            return hg::HealthState::Degraded;
        case homeguard::HardwareModuleState::Fault:
            return hg::HealthState::Failed;
        default:
            return hg::HealthState::Unknown;
    }
}

hg::SystemMode system_mode(const hg::SystemModel& model)
{
    hg::PartitionRecord partition{};
    if (!model.partition_at_snapshot(0, partition)) return hg::SystemMode::Disarmed;
    switch (partition.arm_state) {
        case hg::PartitionArmState::Stay: return hg::SystemMode::ArmedHome;
        case hg::PartitionArmState::Away: return hg::SystemMode::ArmedAway;
        case hg::PartitionArmState::Alarm: return hg::SystemMode::Alarm;
        default: return hg::SystemMode::Disarmed;
    }
}

hg::ZoneState zone_state(const hg::ZoneRecord& zone)
{
    if (!zone.enabled || zone.bypassed || zone.state == hg::ModelZoneState::Bypassed) {
        return hg::ZoneState::Disabled;
    }
    switch (zone.state) {
        case hg::ModelZoneState::Tamper:
            return hg::ZoneState::Tamper;
        case hg::ModelZoneState::Open:
        case hg::ModelZoneState::Alarm:
        case hg::ModelZoneState::Fault:
            return hg::ZoneState::Open;
        default:
            return hg::ZoneState::Normal;
    }
}

std::uint64_t rtc_epoch(Ds3231& rtc, bool& valid)
{
    std::tm value{};
    valid = rtc.read_time(&value) == ESP_OK;
    if (!valid) return 0;
    const auto epoch = std::mktime(&value);
    if (epoch < 0) {
        valid = false;
        return 0;
    }
    return static_cast<std::uint64_t>(epoch);
}

}  // namespace

esp_err_t TelemetryRuntime::start(
    HardwareBootstrap* hardware,
    WebsocketTelemetry* websocket,
    const hg::SystemModel* system_model)
{
    if (hardware == nullptr || websocket == nullptr || system_model == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    hardware_ = hardware;
    websocket_ = websocket;
    system_model_ = system_model;

    const auto result = xTaskCreate(
        &TelemetryRuntime::task_entry,
        "hg_telemetry",
        7168,
        this,
        6,
        nullptr);

    return result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void TelemetryRuntime::task_entry(void* context)
{
    static_cast<TelemetryRuntime*>(context)->run();
}

void TelemetryRuntime::run()
{
    std::uint32_t cycles = 0;
    while (true) {
        const auto now_ms = static_cast<std::uint64_t>(esp_timer_get_time() / 1000);
        const auto& hardware_status = hardware_->status();

        health_.set(hg::Component::Esp, hg::HealthState::Ok, now_ms);
        health_.set(hg::Component::Nvs, hg::HealthState::Ok, now_ms);
        health_.set(hg::Component::Adc1, module_health(hardware_status.ads1115_zones.state), now_ms);
        health_.set(hg::Component::Adc2, module_health(hardware_status.ads1115_telemetry.state), now_ms);
        health_.set(hg::Component::W5500, module_health(hardware_status.w5500.state), now_ms);
        health_.set(hg::Component::Inputs, module_health(hardware_status.mcp23017.state), now_ms);
        health_.set(hg::Component::Outputs, module_health(hardware_status.mcp23017.state), now_ms);

        bool rtc_valid = false;
        std::uint64_t epoch = 0;
        if (hardware_status.ds3231.state == homeguard::HardwareModuleState::Ready &&
            hardware_->rtc().ready()) {
            epoch = rtc_epoch(hardware_->rtc(), rtc_valid);
        }
        health_.set(hg::Component::Rtc,
                    rtc_valid ? hg::HealthState::Ok : module_health(hardware_status.ds3231.state),
                    now_ms);

        wifi_ap_record_t wifi_ap{};
        const bool wifi_connected = esp_wifi_sta_get_ap_info(&wifi_ap) == ESP_OK;
        const auto ethernet_status = hardware_->ethernet().status();
        const auto transport = ethernet_status.link_up && ethernet_status.has_ip
            ? hg::Transport::Ethernet
            : (wifi_connected ? hg::Transport::WifiSta : hg::Transport::EmergencyAp);
        health_.set(hg::Component::Wifi,
                    wifi_connected ? hg::HealthState::Ok : hg::HealthState::Degraded,
                    now_ms);

        std::array<hg::ZoneState, 5> zones{};
        zones.fill(hg::ZoneState::Disabled);
        const auto zone_count = std::min<std::size_t>(zones.size(), system_model_->zone_count());
        for (std::size_t index = 0; index < zone_count; ++index) {
            hg::ZoneRecord zone{};
            if (system_model_->zone_at_snapshot(index, zone)) {
                zones[index] = zone_state(zone);
            }
        }

        std::array<hg::PressureState, 2> pressures{};
        std::array<float, 2> pressure_values{};
        std::array<bool, 2> pressure_valid{};
        auto& analog_adc = hardware_->telemetry_adc();
        for (std::size_t index = 0; index < pressures.size(); ++index) {
            if (hardware_status.ads1115_telemetry.state != homeguard::HardwareModuleState::Ready ||
                !analog_adc.ready()) {
                pressures[index] = hg::PressureState::Disabled;
                continue;
            }
            float millivolts = 0.0F;
            if (analog_adc.read_single_ended_mv(static_cast<std::uint8_t>(index), &millivolts) == ESP_OK) {
                pressure_values[index] = millivolts;
                pressure_valid[index] = true;
                pressures[index] = hg::PressureState::Normal;
            } else {
                pressures[index] = hg::PressureState::SensorFault;
            }
        }

        std::array<float, 8> temperatures{};
        std::array<bool, 8> temperature_valid{};
        std::uint8_t temperature_count = 0;
        auto& one_wire = hardware_->one_wire();
        if (hardware_status.one_wire.state != homeguard::HardwareModuleState::Missing &&
            hardware_status.one_wire.state != homeguard::HardwareModuleState::Fault &&
            one_wire.ready()) {
            const bool rediscovery_due = cycles == 0U || (cycles % kOneWireRediscoveryCycles) == 0U;
            if (one_wire.device_count() == 0U && rediscovery_due) {
                (void)one_wire.discover();
            }
            if (one_wire.device_count() > 0U && one_wire.convert_all() == ESP_OK) {
                (void)one_wire.read_all();
            }
            const auto count = std::min<std::size_t>(one_wire.device_count(), temperatures.size());
            temperature_count = static_cast<std::uint8_t>(count);
            const auto* devices = one_wire.devices();
            for (std::size_t index = 0; index < count; ++index) {
                temperatures[index] = devices[index].temperature_c;
                temperature_valid[index] = devices[index].valid;
            }
        }

        Ina226Reading battery{};
        bool battery_valid = false;
        if (hardware_status.ina226.state == homeguard::HardwareModuleState::Ready &&
            hardware_->battery_monitor().ready()) {
            battery_valid = hardware_->battery_monitor().read(&battery) == ESP_OK;
        }

        const auto frame = builder_.build(
            now_ms,
            epoch,
            system_mode(*system_model_),
            transport,
            zones,
            pressures,
            health_,
            temperatures,
            temperature_valid,
            temperature_count,
            battery.bus_voltage_v,
            battery.current_a,
            battery.power_w,
            battery_valid,
            pressure_values,
            pressure_valid);

        websocket_->publish(frame);

        if ((++cycles % 60U) == 0U && hardware_->storage().status().mounted) {
            (void)hardware_->storage().refresh_space();
        }

        ESP_LOGD(kTag,
                 "telemetry seq=%llu transport=%.*s analog0=%.1fmV analog1=%.1fmV temperatures=%u battery=%s",
                 static_cast<unsigned long long>(frame.sequence),
                 static_cast<int>(hg::to_string(frame.transport).size()),
                 hg::to_string(frame.transport).data(),
                 frame.pressure_values[0],
                 frame.pressure_values[1],
                 static_cast<unsigned>(frame.temperature_count),
                 frame.battery_valid ? "ok" : "fault");

        vTaskDelay(kTelemetryPeriod);
    }
}

}  // namespace homeguard::idf
