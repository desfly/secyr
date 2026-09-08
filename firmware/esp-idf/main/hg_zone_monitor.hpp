#pragma once

#include "hg_ads1115.hpp"
#include "homeguard/system_model.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace homeguard::idf {

enum class ZoneElectricalState : std::uint8_t {
    Normal,
    Open,
    Short,
};

struct ZoneLiveState {
    float millivolts{};
    bool valid{};
    ZoneElectricalState state{ZoneElectricalState::Open};
};

struct ZoneConfig {
    std::array<char, 64> name{};
    float short_max_mv{200.0F};
    float open_min_mv{2730.0F};
};

class ZoneMonitor {
public:
    static constexpr std::size_t kZoneCount = 8;

    esp_err_t start(Ads1115* adc0, Ads1115* adc1, hg::SystemModel* model);
    esp_err_t load();
    esp_err_t save() const;
    esp_err_t set_name(std::size_t index, const std::string& name);
    std::string snapshot_json() const;

private:
    static void task_entry(void* context);
    void run();
    void set_defaults();
    ZoneElectricalState classify(float mv, const ZoneConfig& cfg, ZoneElectricalState previous) const noexcept;

    Ads1115* adc0_{nullptr};
    Ads1115* adc1_{nullptr};
    hg::SystemModel* model_{nullptr};
    SemaphoreHandle_t mutex_{nullptr};
    std::array<ZoneLiveState, kZoneCount> live_{};
    std::array<ZoneConfig, kZoneCount> config_{};
};

const char* zone_electrical_state_name(ZoneElectricalState state) noexcept;

}  // namespace homeguard::idf
