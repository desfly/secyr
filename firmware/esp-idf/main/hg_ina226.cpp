#include "hg_ina226.hpp"
#include "hg_i2c_bus.hpp"
#include "esp_check.h"
#include "esp_log.h"
#include <cstdint>

namespace homeguard::idf {

namespace {

constexpr const char* kTag = "hg_ina226";
constexpr std::uint8_t kConfig = 0x00;
constexpr std::uint8_t kShuntVoltage = 0x01;
constexpr std::uint8_t kBusVoltage = 0x02;
constexpr std::uint8_t kPower = 0x03;
constexpr std::uint8_t kCurrent = 0x04;
constexpr std::uint8_t kCalibration = 0x05;
constexpr std::uint8_t kManufacturerId = 0xFE;
constexpr std::uint8_t kDieId = 0xFF;

void scan_i2c_bus(I2cBus& bus)
{
    ESP_LOGI(kTag, "I2C FULL SCAN START: 0x08..0x77");
    unsigned found = 0;
    for (std::uint16_t address = 0x08; address <= 0x77; ++address) {
        const auto probe = i2c_master_probe(
            bus.handle(),
            static_cast<std::uint8_t>(address),
            30);
        if (probe == ESP_OK) {
            ++found;
            ESP_LOGI(kTag, "I2C FOUND: 0x%02X", static_cast<unsigned>(address));
        }
    }
    ESP_LOGI(kTag, "I2C FULL SCAN DONE: %u device(s) ACK", found);
    ESP_LOGI(kTag, "Expected currently: ADS1115 0x48, ADS1115 0x49, INA226 usually 0x40");
}

}  // namespace

esp_err_t Ina226::initialize(
    I2cBus& bus,
    std::uint8_t address,
    float shunt_ohms,
    float maximum_current_a)
{
    if (initialized_) {
        return ESP_OK;
    }
    if (shunt_ohms <= 0.0F || maximum_current_a <= 0.0F) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!bus.ready() || bus.handle() == nullptr) {
        ESP_LOGE(kTag, "INA226 init aborted: I2C bus not ready");
        return ESP_ERR_INVALID_STATE;
    }

    initialized_ = false;
    device_ = nullptr;

    ESP_LOGI(kTag, "INA226 probe start: address=0x%02X", static_cast<unsigned>(address));
    auto probe_error = i2c_master_probe(bus.handle(), address, 100);

    if (probe_error == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "INA226 probe returned INVALID_STATE; resetting I2C bus and retrying");
        const auto reset_error = i2c_master_bus_reset(bus.handle());
        if (reset_error != ESP_OK) {
            ESP_LOGE(kTag, "I2C bus reset failed: %s", esp_err_to_name(reset_error));
            return reset_error;
        }
        probe_error = i2c_master_probe(bus.handle(), address, 100);
    }

    if (probe_error != ESP_OK) {
        ESP_LOGE(
            kTag,
            "INA226 PROBE FAIL at 0x%02X: %s",
            static_cast<unsigned>(address),
            esp_err_to_name(probe_error));
        scan_i2c_bus(bus);
        return probe_error;
    }
    ESP_LOGI(kTag, "INA226 PROBE ACK: 0x%02X", static_cast<unsigned>(address));

    auto error = bus.add_device(
        address,
        400000,
        &device_);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "INA226 add_device failed: %s", esp_err_to_name(error));
        return error;
    }

    std::uint16_t manufacturer = 0;
    error = read_register(kManufacturerId, &manufacturer);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "INA226 manufacturer-ID read failed: %s", esp_err_to_name(error));
        return error;
    }

    std::uint16_t die_id = 0;
    error = read_register(kDieId, &die_id);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "INA226 die-ID read failed: %s", esp_err_to_name(error));
        return error;
    }

    ESP_LOGI(
        kTag,
        "INA226 identity: manufacturer=0x%04X die=0x%04X",
        static_cast<unsigned>(manufacturer),
        static_cast<unsigned>(die_id));

    current_lsb_a_ = maximum_current_a / 32768.0F;
    power_lsb_w_ = current_lsb_a_ * 25.0F;

    const auto calibration =
        static_cast<std::uint16_t>(
            0.00512F /
            (current_lsb_a_ * shunt_ohms));

    error = write_register(kConfig, 0x4527);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "INA226 CONFIG write failed: %s", esp_err_to_name(error));
        return error;
    }
    ESP_LOGI(kTag, "INA226 CONFIG write OK");

    error = write_register(kCalibration, calibration);
    if (error != ESP_OK) {
        ESP_LOGE(
            kTag,
            "INA226 CALIBRATION write failed (0x%04X): %s",
            static_cast<unsigned>(calibration),
            esp_err_to_name(error));
        return error;
    }
    ESP_LOGI(
        kTag,
        "INA226 CALIBRATION write OK: 0x%04X shunt=%.3f ohm max_current=%.3f A",
        static_cast<unsigned>(calibration),
        static_cast<double>(shunt_ohms),
        static_cast<double>(maximum_current_a));

    initialized_ = true;
    return ESP_OK;
}

esp_err_t Ina226::write_register(
    std::uint8_t reg,
    std::uint16_t value)
{
    const std::uint8_t data[]{
        reg,
        static_cast<std::uint8_t>(value >> 8),
        static_cast<std::uint8_t>(value & 0xFF),
    };
    return i2c_master_transmit(
        device_,
        data,
        sizeof(data),
        100);
}

esp_err_t Ina226::read_register(
    std::uint8_t reg,
    std::uint16_t* value)
{
    if (value == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    std::uint8_t bytes[2]{};
    const auto error = i2c_master_transmit_receive(
        device_,
        &reg,
        1,
        bytes,
        sizeof(bytes),
        100);

    if (error == ESP_OK) {
        *value =
            static_cast<std::uint16_t>(bytes[0] << 8) |
            bytes[1];
    }
    return error;
}

esp_err_t Ina226::read(Ina226Reading* reading)
{
    if (reading == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!initialized_ || device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    std::uint16_t bus_raw = 0;
    std::uint16_t shunt_raw = 0;
    std::uint16_t current_raw = 0;
    std::uint16_t power_raw = 0;

    ESP_RETURN_ON_ERROR(
        read_register(kBusVoltage, &bus_raw),
        "ina226",
        "bus voltage");
    ESP_RETURN_ON_ERROR(
        read_register(kShuntVoltage, &shunt_raw),
        "ina226",
        "shunt voltage");
    ESP_RETURN_ON_ERROR(
        read_register(kCurrent, &current_raw),
        "ina226",
        "current");
    ESP_RETURN_ON_ERROR(
        read_register(kPower, &power_raw),
        "ina226",
        "power");

    reading->bus_voltage_v =
        static_cast<float>(bus_raw) * 0.00125F;
    reading->shunt_voltage_mv =
        static_cast<float>(
            static_cast<std::int16_t>(shunt_raw)) *
        0.0025F;
    reading->current_a =
        static_cast<float>(
            static_cast<std::int16_t>(current_raw)) *
        current_lsb_a_;
    reading->power_w =
        static_cast<float>(power_raw) *
        power_lsb_w_;

    return ESP_OK;
}

bool Ina226::ready() const noexcept
{
    return initialized_;
}

}  // namespace homeguard::idf
