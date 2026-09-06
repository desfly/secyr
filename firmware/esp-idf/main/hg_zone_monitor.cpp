#include "hg_zone_monitor.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "nvs.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "hg_zones";
constexpr const char* kNvsNamespace = "hg_zones";
constexpr const char* kNvsKey = "config_v1";
constexpr TickType_t kPollPeriod = pdMS_TO_TICKS(250);

std::string json_escape(const char* text)
{
    std::string out;
    if (text == nullptr) return out;
    for (const unsigned char ch : std::string{text}) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch >= 0x20U) out.push_back(static_cast<char>(ch));
                break;
        }
    }
    return out;
}

hg::ModelZoneState model_state(ZoneElectricalState state)
{
    switch (state) {
        case ZoneElectricalState::Normal: return hg::ModelZoneState::Normal;
        case ZoneElectricalState::Short: return hg::ModelZoneState::Fault;
        case ZoneElectricalState::Open: return hg::ModelZoneState::Open;
    }
    return hg::ModelZoneState::Open;
}
}

const char* zone_electrical_state_name(ZoneElectricalState state) noexcept
{
    switch (state) {
        case ZoneElectricalState::Normal: return "normal";
        case ZoneElectricalState::Short: return "short";
        case ZoneElectricalState::Open: return "open";
    }
    return "open";
}

void ZoneMonitor::set_defaults()
{
    for (std::size_t i = 0; i < config_.size(); ++i) {
        config_[i] = {};
        std::snprintf(config_[i].name.data(), config_[i].name.size(), "Зона %u", static_cast<unsigned>(i + 1U));
        config_[i].short_max_mv = 500.0F;
        config_[i].open_min_mv = 3000.0F;
    }
}

esp_err_t ZoneMonitor::load()
{
    nvs_handle_t handle{};
    const auto open_error = nvs_open(kNvsNamespace, NVS_READONLY, &handle);
    if (open_error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (open_error != ESP_OK) return open_error;

    std::array<ZoneConfig, kZoneCount> loaded{};
    std::size_t size = sizeof(loaded);
    const auto error = nvs_get_blob(handle, kNvsKey, loaded.data(), &size);
    nvs_close(handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (error != ESP_OK) return error;
    if (size != sizeof(loaded)) return ESP_ERR_INVALID_SIZE;

    for (const auto& item : loaded) {
        if (item.name.back() != '\0' || item.short_max_mv < 0.0F || item.open_min_mv <= item.short_max_mv || item.open_min_mv > 5000.0F) {
            return ESP_ERR_INVALID_RESPONSE;
        }
    }

    if (mutex_ != nullptr) xSemaphoreTake(mutex_, portMAX_DELAY);
    config_ = loaded;
    if (mutex_ != nullptr) xSemaphoreGive(mutex_);
    return ESP_OK;
}

esp_err_t ZoneMonitor::save() const
{
    std::array<ZoneConfig, kZoneCount> copy{};
    if (mutex_ != nullptr) xSemaphoreTake(mutex_, portMAX_DELAY);
    copy = config_;
    if (mutex_ != nullptr) xSemaphoreGive(mutex_);

    nvs_handle_t handle{};
    auto error = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_set_blob(handle, kNvsKey, copy.data(), sizeof(copy));
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

esp_err_t ZoneMonitor::set_name(std::size_t index, const std::string& name)
{
    if (index >= kZoneCount || name.empty() || name.size() >= config_[index].name.size()) return ESP_ERR_INVALID_ARG;
    if (mutex_ != nullptr) xSemaphoreTake(mutex_, portMAX_DELAY);
    std::fill(config_[index].name.begin(), config_[index].name.end(), '\0');
    std::copy(name.begin(), name.end(), config_[index].name.begin());
    if (mutex_ != nullptr) xSemaphoreGive(mutex_);
    return save();
}

ZoneElectricalState ZoneMonitor::classify(float mv, const ZoneConfig& cfg) const noexcept
{
    if (mv <= cfg.short_max_mv) return ZoneElectricalState::Short;
    if (mv >= cfg.open_min_mv) return ZoneElectricalState::Open;
    return ZoneElectricalState::Normal;
}

esp_err_t ZoneMonitor::start(Ads1115* adc0, Ads1115* adc1, hg::SystemModel* model)
{
    if (adc0 == nullptr || adc1 == nullptr) return ESP_ERR_INVALID_ARG;
    if (mutex_ == nullptr) mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) return ESP_ERR_NO_MEM;

    adc0_ = adc0;
    adc1_ = adc1;
    model_ = model;
    set_defaults();
    const auto load_error = load();
    if (load_error != ESP_OK) {
        ESP_LOGW(kTag, "Zone config rejected (%s); defaults retained", esp_err_to_name(load_error));
        set_defaults();
    }

    const auto created = xTaskCreate(&ZoneMonitor::task_entry, "hg_zones", 4096, this, 6, nullptr);
    return created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void ZoneMonitor::task_entry(void* context)
{
    static_cast<ZoneMonitor*>(context)->run();
}

void ZoneMonitor::run()
{
    std::array<ZoneLiveState, kZoneCount> next{};
    while (true) {
        for (std::size_t zone = 0; zone < kZoneCount; ++zone) {
            Ads1115* adc = zone < 4U ? adc0_ : adc1_;
            const auto channel = static_cast<std::uint8_t>(zone % 4U);
            float mv = 0.0F;
            const bool valid = adc != nullptr && adc->ready() && adc->read_single_ended_mv(channel, &mv) == ESP_OK;

            ZoneConfig cfg{};
            xSemaphoreTake(mutex_, portMAX_DELAY);
            cfg = config_[zone];
            xSemaphoreGive(mutex_);

            next[zone].millivolts = valid ? mv : 0.0F;
            next[zone].valid = valid;
            next[zone].state = valid ? classify(mv, cfg) : ZoneElectricalState::Open;

            if (model_ != nullptr) {
                (void)model_->set_zone_state(
                    static_cast<std::uint16_t>(zone + 1U),
                    model_state(next[zone].state),
                    static_cast<std::uint64_t>(esp_timer_get_time() / 1000));
            }
        }

        xSemaphoreTake(mutex_, portMAX_DELAY);
        live_ = next;
        xSemaphoreGive(mutex_);
        vTaskDelay(kPollPeriod);
    }
}

std::string ZoneMonitor::snapshot_json() const
{
    std::array<ZoneLiveState, kZoneCount> live{};
    std::array<ZoneConfig, kZoneCount> config{};
    if (mutex_ != nullptr) xSemaphoreTake(mutex_, portMAX_DELAY);
    live = live_;
    config = config_;
    if (mutex_ != nullptr) xSemaphoreGive(mutex_);

    std::ostringstream out;
    out << "{\"ok\":true,\"zones\":[" << std::fixed << std::setprecision(1);
    for (std::size_t i = 0; i < kZoneCount; ++i) {
        if (i != 0U) out << ',';
        out << "{\"id\":" << (i + 1U)
            << ",\"name\":\"" << json_escape(config[i].name.data()) << "\""
            << ",\"mv\":" << live[i].millivolts
            << ",\"valid\":" << (live[i].valid ? "true" : "false")
            << ",\"state\":\"" << zone_electrical_state_name(live[i].state) << "\"}";
    }
    out << "],\"thresholds\":{\"note\":\"provisional_until_calibration\"}}";
    return out.str();
}

}  // namespace homeguard::idf
