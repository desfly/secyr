#include "hg_telemetry_runtime.hpp"
#include "hg_cloud_link.hpp"
#include "hg_hardware_bootstrap.hpp"
#include "homeguard/system_model.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "hg_telemetry";
constexpr std::uint64_t kHeartbeatMs = 15000U;
constexpr TickType_t kChangePollTicks = pdMS_TO_TICKS(250);
constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

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

void hash_byte(std::uint64_t& hash, std::uint8_t value)
{
    hash ^= value;
    hash *= kFnvPrime;
}

template <typename T>
void hash_scalar(std::uint64_t& hash, T value)
{
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    for (std::size_t i = 0; i < sizeof(T); ++i) hash_byte(hash, bytes[i]);
}

std::uint64_t model_fingerprint(const hg::SystemModel& model)
{
    std::uint64_t hash = kFnvOffset;

    if (const auto* partition = model.partition(1); partition != nullptr) {
        hash_scalar(hash, static_cast<std::uint8_t>(partition->arm_state));
    } else {
        hash_byte(hash, 0xffU);
    }

    hash_scalar(hash, model.zone_count());
    for (std::size_t i = 0; i < model.zone_count(); ++i) {
        const auto* zone = model.zone_at(i);
        if (zone == nullptr) continue;
        hash_scalar(hash, zone->id);
        hash_scalar(hash, static_cast<std::uint8_t>(zone->state));
        hash_scalar(hash, zone->enabled);
        hash_scalar(hash, zone->bypassed);
        hash_scalar(hash, zone->always_on);
    }

    hash_scalar(hash, model.sensor_count());
    for (std::size_t i = 0; i < model.sensor_count(); ++i) {
        const auto* sensor = model.sensor_at(i);
        if (sensor == nullptr) continue;
        hash_scalar(hash, sensor->id);
        hash_scalar(hash, sensor->online);
        hash_scalar(hash, sensor->battery_percent);
        hash_scalar(hash, sensor->rssi_dbm);
        hash_scalar(hash, sensor->last_seen_ms);
    }

    hash_scalar(hash, model.output_count());
    for (std::size_t i = 0; i < model.output_count(); ++i) {
        const auto* output = model.output_at(i);
        if (output == nullptr) continue;
        hash_scalar(hash, output->id);
        hash_scalar(hash, output->active);
    }

    return hash;
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
    bool first = true;
    for (std::size_t i = 0; i < model_->zone_count(); ++i) {
        const auto* zone = model_->zone_at(i);
        if (zone == nullptr) continue;
        if (!first) body += ',';
        first = false;
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
    first = true;
    for (std::size_t i = 0; i < model_->sensor_count(); ++i) {
        const auto* sensor = model_->sensor_at(i);
        if (sensor == nullptr) continue;
        if (!first) body += ',';
        first = false;
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
    first = true;
    for (std::size_t i = 0; i < model_->output_count(); ++i) {
        const auto* output = model_->output_at(i);
        if (output == nullptr) continue;
        if (!first) body += ',';
        first = false;
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
    std::uint64_t last_published_fingerprint = 0U;
    std::uint64_t last_publish_ms = 0U;
    std::uint64_t last_hardware_sample_ms = 0U;
    bool have_published_fingerprint = false;

    while (true) {
        const auto now = static_cast<std::uint64_t>(esp_timer_get_time() / 1000);

        if (last_hardware_sample_ms == 0U || now - last_hardware_sample_ms >= kHeartbeatMs) {
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
            last_hardware_sample_ms = now;
        }

        const auto fingerprint = model_fingerprint(*model_);
        const bool state_changed = !have_published_fingerprint || fingerprint != last_published_fingerprint;
        const bool heartbeat_due = last_publish_ms == 0U || now - last_publish_ms >= kHeartbeatMs;

        if (state_changed || heartbeat_due) {
            const auto cloud_error = publish_now();
            if (cloud_error == ESP_OK) {
                last_published_fingerprint = fingerprint;
                have_published_fingerprint = true;
                last_publish_ms = now;
            } else if (cloud_error != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(kTag, "Cloud state publish failed: %s", esp_err_to_name(cloud_error));
            }
        }

        vTaskDelay(kChangePollTicks);
    }
}

}  // namespace homeguard::idf
