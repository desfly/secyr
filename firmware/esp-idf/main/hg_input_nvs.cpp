#include "hg_input_nvs.hpp"

#include "nvs.h"

#include <cstdint>

namespace homeguard::idf {
namespace {
constexpr const char* kNamespace = "hg_inputs";
constexpr const char* kTamperKey = "tamper_pol";
constexpr const char* kPowerFailKey = "power_pol";

std::uint8_t encode(InputPolarity polarity)
{
    switch (polarity) {
        case InputPolarity::ActiveHigh: return 1;
        case InputPolarity::ActiveLow: return 2;
        default: return 0;
    }
}

InputPolarity decode(std::uint8_t value)
{
    switch (value) {
        case 1: return InputPolarity::ActiveHigh;
        case 2: return InputPolarity::ActiveLow;
        default: return InputPolarity::Unknown;
    }
}
}

esp_err_t InputNvsStore::load(InputPolarityConfig& config) const
{
    config = {};
    nvs_handle_t handle{};
    const auto open_error = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (open_error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (open_error != ESP_OK) return open_error;

    std::uint8_t tamper{};
    std::uint8_t power_fail{};
    const auto tamper_error = nvs_get_u8(handle, kTamperKey, &tamper);
    const auto power_error = nvs_get_u8(handle, kPowerFailKey, &power_fail);
    nvs_close(handle);

    if (tamper_error != ESP_OK && tamper_error != ESP_ERR_NVS_NOT_FOUND) return tamper_error;
    if (power_error != ESP_OK && power_error != ESP_ERR_NVS_NOT_FOUND) return power_error;

    if (tamper_error == ESP_OK) config.tamper = decode(tamper);
    if (power_error == ESP_OK) config.power_fail = decode(power_fail);
    return ESP_OK;
}

esp_err_t InputNvsStore::save(const InputPolarityConfig& config) const
{
    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;

    error = nvs_set_u8(handle, kTamperKey, encode(config.tamper));
    if (error == ESP_OK) error = nvs_set_u8(handle, kPowerFailKey, encode(config.power_fail));
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

}  // namespace homeguard::idf
