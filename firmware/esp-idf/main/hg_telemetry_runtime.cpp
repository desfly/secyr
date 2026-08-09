#include "hg_telemetry_runtime.hpp"
#include "hg_cloud_link.hpp"
#include "hg_hardware_bootstrap.hpp"
#include "homeguard/system_model.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstddef>
#include <string>

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "hg_telemetry";

const char* arm_state_name(hg::PartitionArmState state)
{
    switch (state) {
        case hg::PartitionArmState::Disarmed: return "disarmed";
        case hg::PartitionArmState::Stay: return "armed_home";
        case hg::PartitionArmState::Away: return "armed_away";
        case hg::PartitionArmState::Alarm: return "alarm";
    }
    return "unknown";
}

const char* zone_state_name(hg::ModelZoneState state)
{
    switch (state) {
        case hg::ModelZoneState::Normal: return "normal";
        case hg::ModelZoneState::Open: return "open";
        case hg::ModelZoneState::Alarm: return "alarm";
        case hg::ModelZoneState::Fault: return "fault";
        case hg::ModelZoneState::Tamper: return "tamper";
        case hg::ModelZoneState::Bypassed: return "bypassed";
    }
    return "unknown";
}

std::string json_escape(const char* value)
{
    std::string out;
    if (value == nullptr) return out;
    for (const char* p = value; *p != '\0'; ++p) {
        if (*p == '"' || *p == '\\') out.push_back('\\');
        out.push_back(*p);
    }
    return out;
}
}  // namespace

esp_err_t TelemetryRuntime::start(HardwareBootstrap* hardware,
                                  hg::SystemModel* model,
                                  CloudLink* cloud)
{
    if (hardware == nullptr || model == nullptr || cloud == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    hardware_ = hardware;
    model_ = model;
    cloud_ = cloud;

    const auto result = xTaskCreate(
        &TelemetryRuntime::task_entry,
        "hg_telemetry",
        8192,
        this,
        6,
        nullptr);

    return result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t TelemetryRuntime::publish_now()
{
    if (model_ == nullptr || cloud_ == nullptr || !cloud_->connected()) {
        return ESP_ERR_INVALID_STATE;
    }

    std::string body = "{\"schema\":1,\"sequence\":" + std::to_string(++sequence_);
    body += ",\"timestamp_ms\":" +
            std::to_string(static_cast<unsigned long long>(esp_timer_get_time() / 1000));

    const auto* partition = model_->partition(1);
    body += ",\"arm_state\":\"";
    body += partition == nullptr ? "unknown" : arm_state_name(partition->arm_state);
    body += '"';

    body += ",\"zones\":[";
    for (std::size_t i = 0; i < model_->zone_count(); ++i) {
        const auto* zone = model_->zone_at(i);
        if (zone == nullptr) continue;
        if (i != 0U) body += ',';
        body += "{\"id\":" + std::to_string(zone->id);
        body += ",\"name\":\"" + json_escape(zone->name.data()) + "\"";
        body += ",\"state\":\"";
        body += zone_state_name(zone->state);
        body += "\",\"enabled\":";
        body += zone->enabled ? "true" : "false";
        body += ",\"bypassed\":";
        body += zone->bypassed ? "true" : "false";
        body += ",\"always_on\":";
        body += zone->always_on ? "true" : "false";
        body += '}';
    }
    body += ']';

    body += ",\"sensors\":[";
    for (std::size_t i = 0; i < model_->sensor_count(); ++i) {
        const auto* sensor = model_->sensor_at(i);
        if (sensor == nullptr) continue;
        if (i != 0U) body += ',';
        body += "{\"id\":" + std::to_string(sensor->id);
        body += ",\"online\":";
        body += sensor->online ? "true" : "false";
        body += ",\"battery_percent\":" + std::to_string(sensor->battery_percent);
        body += ",\"rssi_dbm\":" + std::to_string(sensor->rssi_dbm);
        body += ",\"last_seen_ms\":" +
                std::to_string(static_cast<unsigned long long>(sensor->last_seen_ms));
        body += '}';
    }
    body += ']';

    body += ",\"outputs\":[";
    for (std::size_t i = 0; i < model_->output_count(); ++i) {
        const auto* output = model_->output_at(i);
        if (output == nullptr) continue;
        if (i != 0U) body += ',';
        body += "{\"id\":" + std::to_string(output->id);
        body += ",\"active\":";
        body += output->active ? "true" : "false";
        body += '}';
    }
    body += "]}";

    const auto error = cloud_->publish_state(body.c_str(), 1, true);
    if (error == ESP_OK) {
        ESP_LOGI(kTag, "Cloud state published: sequence=%llu", sequence_);
    }
    return error;
}

void TelemetryRuntime::task_entry(void* context)
{
    static_cast<TelemetryRuntime*>(context)->run();
}

void TelemetryRuntime::run()
{
    while (true) {
        Ina226Reading battery{};
        const auto battery_error = hardware_->battery_monitor().read(&battery);
        if (battery_error == ESP_OK) {
            ESP_LOGI(kTag,
                     "battery voltage=%.3f current=%.3f power=%.3f",
                     static_cast<double>(battery.bus_voltage_v),
                     static_cast<double>(battery.current_a),
                     static_cast<double>(battery.power_w));
        }

        float rtc_temperature = 0.0F;
        (void)hardware_->rtc().read_temperature(&rtc_temperature);
        (void)hardware_->storage().refresh_space();

        const auto cloud_error = publish_now();
        if (cloud_error != ESP_OK && cloud_error != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Cloud state publish failed: %s", esp_err_to_name(cloud_error));
        }

        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

}  // namespace homeguard::idf
