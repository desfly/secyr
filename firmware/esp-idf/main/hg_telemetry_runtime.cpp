#include "hg_telemetry_runtime.hpp"
#include "hg_hardware_bootstrap.hpp"
#include "hg_ble_transport.hpp"
#include "websocket_telemetry.hpp"
#include "homeguard/system_model.hpp"
#include "homeguard/hardware_calibration.hpp"
#include "homeguard/hardware_profile.hpp"

#include "driver/gpio.h"
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
constexpr std::uint16_t kLightOutputId = 4;
constexpr std::uint64_t kLightCycleMs = 60'000ULL;

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
    const auto* partition = model.partition_at(0);
    if (partition == nullptr) return hg::SystemMode::Disarmed;
    switch (partition->arm_state) {
        case hg::PartitionArmState::Stay: return hg::SystemMode::ArmedHome;
        case hg::PartitionArmState::Away: return hg::SystemMode::ArmedAway;
        case hg::PartitionArmState::Alarm: return hg::SystemMode::Alarm;
        default: return hg::SystemMode::Disarmed;
    }
}

hg::ZoneState physical_zone_state(float millivolts)
{
    const homeguard::ZoneCalibration calibration{};
    if (millivolts <= calibration.short_max_mv) return hg::ZoneState::Short;
    if (millivolts >= calibration.open_min_mv) return hg::ZoneState::Open;
    if (millivolts >= calibration.normal_min_mv && millivolts <= calibration.normal_max_mv) {
        return hg::ZoneState::Normal;
    }
    return millivolts < calibration.normal_min_mv ? hg::ZoneState::Short : hg::ZoneState::Open;
}

bool zone_triggers_light(hg::ZoneState state)
{
    return state == hg::ZoneState::Open ||
           state == hg::ZoneState::Short ||
           state == hg::ZoneState::Tamper;
}

void sample_zone_adc(Ads1115& adc, std::size_t first_zone, std::array<hg::ZoneState, 8>& zones)
{
    for (std::size_t channel = 0; channel < 4; ++channel) {
        const auto zone_index = first_zone + channel;
        if (zone_index >= zones.size()) break;
        if (!adc.ready()) { zones[zone_index] = hg::ZoneState::Disabled; continue; }
        float millivolts = 0.0F;
        const auto error = adc.read_single_ended_mv(static_cast<std::uint8_t>(channel), &millivolts);
        zones[zone_index] = error == ESP_OK ? physical_zone_state(millivolts) : hg::ZoneState::Disabled;
    }
}

std::uint64_t rtc_epoch(Ds3231& rtc, bool& valid)
{
    std::tm value{};
    valid = rtc.read_time(&value) == ESP_OK;
    if (!valid) return 0;
    const auto epoch = std::mktime(&value);
    if (epoch < 0) { valid = false; return 0; }
    return static_cast<std::uint64_t>(epoch);
}

}  // namespace

esp_err_t TelemetryRuntime::start(
    HardwareBootstrap* hardware,
    WebsocketTelemetry* websocket,
    hg::SystemModel* system_model,
    BleTransport* ble_transport)
{
    if (hardware == nullptr || websocket == nullptr || system_model == nullptr) return ESP_ERR_INVALID_ARG;
    hardware_ = hardware;
    websocket_ = websocket;
    system_model_ = system_model;
    ble_transport_ = ble_transport;
    const auto result = xTaskCreate(&TelemetryRuntime::task_entry, "hg_telemetry", 7168, this, 6, nullptr);
    return result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void TelemetryRuntime::task_entry(void* context)
{
    static_cast<TelemetryRuntime*>(context)->run();
}

bool TelemetryRuntime::set_light_output(bool active, std::uint64_t now_ms)
{
    if (system_model_ == nullptr) return false;
    const auto* output = system_model_->output(kLightOutputId);
    if (output == nullptr) return false;

    if (output->active != active && !system_model_->set_output_active(kLightOutputId, active, now_ms)) {
        ESP_LOGE(kTag, "Zone light: failed to update output model");
        return false;
    }

    const auto gpio = static_cast<gpio_num_t>(hg::direct_light_relay_gpio);
    if (gpio_set_level(gpio, active ? 1 : 0) != ESP_OK) {
        ESP_LOGE(kTag, "Zone light: GPIO%d write failed", hg::direct_light_relay_gpio);
        return false;
    }
    return true;
}

void TelemetryRuntime::update_zone_light(
    const std::array<hg::ZoneState, 8>& zones,
    std::uint64_t now_ms)
{
    const bool triggered = zone_triggers_light(zones[0]) || zone_triggers_light(zones[1]);

    if (!light_cycle_active_) {
        if (!triggered) return;

        const auto* light = system_model_->output(kLightOutputId);
        if (light == nullptr) {
            ESP_LOGE(kTag, "Zone light: output 4 is missing");
            return;
        }

        light_restore_active_ = light->active;
        if (!set_light_output(true, now_ms)) return;

        light_cycle_active_ = true;
        light_cycle_deadline_ms_ = now_ms + kLightCycleMs;
        ESP_LOGI(kTag, "Zone light: zones 1/2 triggered, light ON for 60 s");
        return;
    }

    // Never shorten or restart the running minute because of transitions that
    // happen inside it. If something turns the relay off during the automatic
    // cycle, assert the required ON state again on the next telemetry tick.
    const auto* light = system_model_->output(kLightOutputId);
    if (now_ms < light_cycle_deadline_ms_) {
        if (light != nullptr && !light->active) (void)set_light_output(true, now_ms);
        return;
    }

    if (triggered) {
        // Still active after one minute: continue with the next minute without
        // dropping the lamp between cycles.
        light_cycle_deadline_ms_ = now_ms + kLightCycleMs;
        if (light != nullptr && !light->active) (void)set_light_output(true, now_ms);
        ESP_LOGI(kTag, "Zone light: trigger still active, next 60 s cycle started");
        return;
    }

    // Both trigger zones returned to normal: the already-started minute has
    // completed, so restore the state that existed before the cycle began.
    if (!set_light_output(light_restore_active_, now_ms)) return;
    light_cycle_active_ = false;
    light_cycle_deadline_ms_ = 0;
    ESP_LOGI(kTag, "Zone light: 60 s cycle complete, previous light state restored");
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
        const auto epoch = rtc_epoch(hardware_->rtc(), rtc_valid);
        health_.set(hg::Component::Rtc, rtc_valid ? hg::HealthState::Ok : module_health(hardware_status.ds3231.state), now_ms);

        wifi_ap_record_t wifi_ap{};
        const bool wifi_connected = esp_wifi_sta_get_ap_info(&wifi_ap) == ESP_OK;
        const auto ethernet_status = hardware_->ethernet().status();
        const auto transport = ethernet_status.link_up && ethernet_status.has_ip ? hg::Transport::Ethernet : (wifi_connected ? hg::Transport::WifiSta : hg::Transport::EmergencyAp);
        health_.set(hg::Component::Wifi, wifi_connected ? hg::HealthState::Ok : hg::HealthState::Degraded, now_ms);

        std::array<hg::ZoneState, 8> zones{};
        zones.fill(hg::ZoneState::Disabled);
        sample_zone_adc(hardware_->zone_adc(), 0, zones);
        sample_zone_adc(hardware_->telemetry_adc(), 4, zones);
        update_zone_light(zones, now_ms);

        std::array<hg::PressureState, 2> pressures{};
        std::array<float, 2> pressure_values{};
        std::array<bool, 2> pressure_valid{};
        auto& analog_adc = hardware_->telemetry_adc();
        for (std::size_t index = 0; index < pressures.size(); ++index) {
            if (!analog_adc.ready()) { pressures[index] = hg::PressureState::Disabled; continue; }
            float millivolts = 0.0F;
            if (analog_adc.read_single_ended_mv(static_cast<std::uint8_t>(index), &millivolts) == ESP_OK) {
                pressure_values[index] = millivolts;
                pressure_valid[index] = true;
                pressures[index] = hg::PressureState::Normal;
            } else pressures[index] = hg::PressureState::SensorFault;
        }

        std::array<float, 8> temperatures{};
        std::array<bool, 8> temperature_valid{};
        std::uint8_t temperature_count = 0;
        auto& one_wire = hardware_->one_wire();
        if (one_wire.ready()) {
            if (one_wire.device_count() == 0U) (void)one_wire.discover();
            if (one_wire.device_count() > 0U && one_wire.convert_all() == ESP_OK) (void)one_wire.read_all();
            const auto count = std::min<std::size_t>(one_wire.device_count(), temperatures.size());
            temperature_count = static_cast<std::uint8_t>(count);
            const auto* devices = one_wire.devices();
            for (std::size_t index = 0; index < count; ++index) {
                temperatures[index] = devices[index].temperature_c;
                temperature_valid[index] = devices[index].valid;
            }
        }

        Ina226Reading battery{};
        auto& battery_monitor = hardware_->battery_monitor();
        const bool battery_valid = battery_monitor.ready() && battery_monitor.read(&battery) == ESP_OK;

        const auto frame = builder_.build(now_ms, epoch, system_mode(*system_model_), transport, zones, pressures, health_, temperatures, temperature_valid, temperature_count, battery.bus_voltage_v, battery.current_a, battery.power_w, battery_valid, pressure_values, pressure_valid);

        websocket_->publish(frame);
        if (ble_transport_ != nullptr && ble_transport_->connected()) {
            const auto ble_error = ble_transport_->publish_telemetry(frame);
            if (ble_error != ESP_OK) ESP_LOGW(kTag, "BLE telemetry publish failed: %s", esp_err_to_name(ble_error));
        }

        if ((++cycles % 60U) == 0U) hardware_->storage().refresh_space();

        ESP_LOGD(kTag,
                 "telemetry seq=%llu transport=%.*s zones=[%u,%u,%u,%u,%u,%u,%u,%u] temperatures=%u battery=%s ble=%s",
                 static_cast<unsigned long long>(frame.sequence),
                 static_cast<int>(hg::to_string(frame.transport).size()),
                 hg::to_string(frame.transport).data(),
                 static_cast<unsigned>(frame.zones[0]), static_cast<unsigned>(frame.zones[1]),
                 static_cast<unsigned>(frame.zones[2]), static_cast<unsigned>(frame.zones[3]),
                 static_cast<unsigned>(frame.zones[4]), static_cast<unsigned>(frame.zones[5]),
                 static_cast<unsigned>(frame.zones[6]), static_cast<unsigned>(frame.zones[7]),
                 static_cast<unsigned>(frame.temperature_count), frame.battery_valid ? "ok" : "fault",
                 ble_transport_ != nullptr && ble_transport_->connected() ? "connected" : "offline");

        vTaskDelay(kTelemetryPeriod);
    }
}

}  // namespace homeguard::idf
