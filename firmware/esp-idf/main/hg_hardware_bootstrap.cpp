#include "hg_hardware_bootstrap.hpp"
#include "hg_board_hw678.hpp"
#include "sdkconfig.h"

#include "esp_log.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>

namespace homeguard::idf {

namespace {

constexpr const char* kTag = "hg_hardware";

void append_adc_snapshot(
    std::ostringstream& output,
    const char* role,
    std::uint8_t expected_address,
    Ads1115& adc)
{
    std::array<float, 4> values{};
    std::array<bool, 4> valid{};
    const auto read_error = adc.ready()
        ? adc.read_all_single_ended_mv(&values, &valid)
        : ESP_ERR_INVALID_STATE;

    output << "{\"role\":\"" << role << "\","
           << "\"address\":" << static_cast<unsigned>(expected_address) << ","
           << "\"address_hex\":\"0x"
           << std::uppercase << std::hex << static_cast<unsigned>(expected_address)
           << std::nouppercase << std::dec << "\","
           << "\"ready\":" << (adc.ready() ? "true" : "false") << ","
           << "\"read_ok\":" << (read_error == ESP_OK ? "true" : "false") << ","
           << "\"channels_mv\":[";

    output << std::fixed << std::setprecision(3);
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0U) output << ',';
        if (valid[index]) output << values[index];
        else output << "null";
    }
    output << "]}";
}

void validate_adc_conversion(
    const char* label,
    Ads1115& adc,
    HardwareModuleStatus& status)
{
    if (!adc.ready()) return;

    float sample_mv = 0.0F;
    const auto error = adc.read_single_ended_mv(0, &sample_mv);
    if (error != ESP_OK) {
        status = {
            HardwareModuleState::Degraded,
            std::string(label) + " detected but conversion read failed",
            1,
        };
        ESP_LOGE(kTag, "TEST FAIL: %s conversion: %s", label, esp_err_to_name(error));
        return;
    }

    ESP_LOGI(kTag, "TEST PASS: %s A0=%.3f mV", label, static_cast<double>(sample_mv));
}

}  // namespace

HardwareModuleStatus HardwareBootstrap::module_status(
    esp_err_t error,
    const char* ready_detail,
    const char* failure_detail)
{
    if (error == ESP_OK) {
        return {
            HardwareModuleState::Ready,
            ready_detail,
            0,
        };
    }
    return {
        HardwareModuleState::Missing,
        failure_detail,
        1,
    };
}

esp_err_t HardwareBootstrap::initialize()
{
    status_.safe_outputs_forced = true;
    ESP_LOGI(kTag, "========== 5-MODULE BENCH TEST START ==========");
    ESP_LOGI(kTag, "Expected: ADS1115@0x48, ADS1115@0x49, INA226@0x40, microSD, W5500");

    auto error = i2c_.initialize();
    status_.i2c = module_status(
        error,
        "GPIO4/GPIO5, 400 kHz devices",
        "I2C bus initialization failed");

    if (error != ESP_OK) {
        ESP_LOGE(kTag, "TEST FAIL: I2C bus GPIO4/GPIO5: %s", esp_err_to_name(error));
        return error;
    }
    ESP_LOGI(kTag, "I2C READY: SDA=GPIO4 SCL=GPIO5 400kHz");

    const auto zone_error = zone_adc_.initialize(i2c_, 0x48);
    status_.ads1115_zones = module_status(
        zone_error,
        "ADS1115 #1 0x48, A0-A3 raw mV",
        "ADS1115 #1 0x48 not detected");
    if (zone_error == ESP_OK) {
        validate_adc_conversion("ADS1115 #1 0x48", zone_adc_, status_.ads1115_zones);
    } else {
        ESP_LOGE(kTag, "TEST FAIL: ADS1115 #1 0x48 not detected: %s", esp_err_to_name(zone_error));
    }

    const auto telemetry_error = telemetry_adc_.initialize(i2c_, 0x49);
    status_.ads1115_telemetry = module_status(
        telemetry_error,
        "ADS1115 #2 0x49, A0-A3 raw mV",
        "ADS1115 #2 0x49 not detected");
    if (telemetry_error == ESP_OK) {
        validate_adc_conversion("ADS1115 #2 0x49", telemetry_adc_, status_.ads1115_telemetry);
    } else {
        ESP_LOGE(kTag, "TEST FAIL: ADS1115 #2 0x49 not detected: %s", esp_err_to_name(telemetry_error));
    }

    const auto mcp_error = mcp23017_.initialize(i2c_, 0x20);
    status_.mcp23017 = module_status(
        mcp_error,
        "MCP23017 0x20, outputs forced OFF",
        "MCP23017 0x20 not detected");

    if (mcp_error == ESP_OK) {
        status_.safe_outputs_forced = mcp23017_.force_safe_outputs() == ESP_OK;
    }

    // CJMCU-226 board fitted in HomeGuard-S3 has R100 = 0.100 ohm.
    const auto ina_error = ina226_.initialize(i2c_, 0x40, 0.100F, 0.8F);
    status_.ina226 = module_status(
        ina_error,
        "INA226 0x40, R100 100 mOhm",
        "INA226 0x40 not detected");

    if (ina_error == ESP_OK) {
        Ina226Reading reading{};
        const auto read_error = ina226_.read(&reading);
        if (read_error == ESP_OK) {
            ESP_LOGI(
                kTag,
                "TEST PASS: INA226 0x40 bus=%.4fV shunt=%.4fmV current=%.5fA power=%.5fW",
                static_cast<double>(reading.bus_voltage_v),
                static_cast<double>(reading.shunt_voltage_mv),
                static_cast<double>(reading.current_a),
                static_cast<double>(reading.power_w));
        } else {
            status_.ina226 = {
                HardwareModuleState::Degraded,
                "INA226 0x40 detected but measurement read failed",
                1,
            };
            ESP_LOGE(kTag, "TEST FAIL: INA226 0x40 read: %s", esp_err_to_name(read_error));
        }
    } else {
        ESP_LOGE(kTag, "TEST FAIL: INA226 0x40 not detected: %s", esp_err_to_name(ina_error));
    }

    const auto rtc_error = ds3231_.initialize(i2c_, 0x68);
    status_.ds3231 = module_status(
        rtc_error,
        "DS3231 0x68",
        "DS3231 0x68 not detected");

    const auto sd_error = sd_storage_.mount();
    status_.micro_sd = module_status(
        sd_error,
        "microSD mounted at /sdcard",
        "microSD mount failed");

    if (sd_error == ESP_OK) {
        const auto sd_test_error = sd_storage_.self_test();
        if (sd_test_error == ESP_OK) {
            status_.micro_sd = {
                HardwareModuleState::Ready,
                "microSD mount + WRITE/READ self-test passed",
                0,
            };
            ESP_LOGI(kTag, "TEST PASS: microSD mount + WRITE + READ + VERIFY");
        } else {
            status_.micro_sd = {
                HardwareModuleState::Degraded,
                "microSD mounted but WRITE/READ self-test failed",
                1,
            };
            ESP_LOGE(kTag, "TEST FAIL: microSD WRITE/READ: %s", esp_err_to_name(sd_test_error));
        }
    } else {
        ESP_LOGE(kTag, "TEST FAIL: microSD mount: %s", esp_err_to_name(sd_error));
    }

    const auto eth_error = w5500_.initialize();
    status_.w5500 = module_status(
        eth_error,
        "W5500 initialized",
        "W5500 initialization failed");

    if (eth_error == ESP_OK) {
        const auto start_error = w5500_.start();
        if (start_error != ESP_OK) {
            status_.w5500 = {
                HardwareModuleState::Degraded,
                "W5500 initialized but start failed",
                1,
            };
            ESP_LOGE(kTag, "TEST FAIL: W5500 start: %s", esp_err_to_name(start_error));
        } else {
            ESP_LOGI(kTag, "W5500 SPI TEST PASS: waiting for LINK UP and IPv4");
        }
    } else {
        ESP_LOGE(kTag, "TEST FAIL: W5500 initialize: %s", esp_err_to_name(eth_error));
    }

    ESP_LOGI(kTag, "========== 5-MODULE BENCH TEST BOOT PHASE DONE ==========");
    ESP_LOGI(kTag, "W5500 final PASS is LINK UP + IPv4 in subsequent log lines");

    const auto one_wire_error = one_wire_.initialize();
    status_.one_wire = module_status(
        one_wire_error,
        "1-Wire initialized on GPIO6",
        "1-Wire initialization failed");

    if (one_wire_error == ESP_OK) {
        const auto discovery_error = one_wire_.discover();
        if (discovery_error != ESP_OK) {
            status_.one_wire = {
                HardwareModuleState::Degraded,
                "1-Wire ready, no DS18B20 discovered",
                1,
            };
        }
    }

    const auto rs485_error = rs485_.initialize(9600);
    status_.rs485 = module_status(
        rs485_error,
        "RS-485 UART1 9600 8N1 half-duplex",
        "RS-485 initialization failed");

#if CONFIG_HOMEGUARD_PZEM004T_TX_GPIO >= 0 && CONFIG_HOMEGUARD_PZEM004T_RX_GPIO >= 0
    const auto pzem_error = pzem004t_.initialize(
        UART_NUM_2,
        CONFIG_HOMEGUARD_PZEM004T_TX_GPIO,
        CONFIG_HOMEGUARD_PZEM004T_RX_GPIO,
        static_cast<std::uint8_t>(CONFIG_HOMEGUARD_PZEM004T_ADDRESS));
    if (pzem_error == ESP_OK) {
        Pzem004tReading reading{};
        const auto read_error = pzem004t_.read(&reading);
        if (read_error == ESP_OK) {
            ESP_LOGI(
                kTag,
                "PZEM PASS: U=%.1fV I=%.3fA P=%.1fW E=%.3fkWh F=%.1fHz PF=%.2f alarm=%s",
                static_cast<double>(reading.voltage_v),
                static_cast<double>(reading.current_a),
                static_cast<double>(reading.active_power_w),
                static_cast<double>(reading.energy_kwh),
                static_cast<double>(reading.frequency_hz),
                static_cast<double>(reading.power_factor),
                reading.power_alarm ? "yes" : "no");
        } else {
            ESP_LOGE(kTag, "PZEM read failed: %s", esp_err_to_name(read_error));
        }
    } else {
        ESP_LOGE(kTag, "PZEM UART2 initialize failed: %s", esp_err_to_name(pzem_error));
    }
#else
    ESP_LOGI(kTag, "PZEM-004T driver integrated but disabled: TX/RX GPIOs are not assigned in canonical HW map");
#endif

    return ESP_OK;
}

const HardwareRuntimeStatus& HardwareBootstrap::status() const noexcept
{
    return status_;
}

std::string HardwareBootstrap::analog_snapshot_json()
{
    std::ostringstream output;
    output << "{\"ok\":"
           << (zone_adc_.ready() && telemetry_adc_.ready() ? "true" : "false")
           << ",\"bus\":{\"sda_gpio\":" << static_cast<int>(board::kI2cSda)
           << ",\"scl_gpio\":" << static_cast<int>(board::kI2cScl)
           << "},\"devices\":[";

    append_adc_snapshot(output, "zones", 0x48, zone_adc_);
    output << ',';
    append_adc_snapshot(output, "telemetry", 0x49, telemetry_adc_);
    output << "]}";
    return output.str();
}

Ads1115& HardwareBootstrap::zone_adc() noexcept { return zone_adc_; }
Ads1115& HardwareBootstrap::telemetry_adc() noexcept { return telemetry_adc_; }
Mcp23017& HardwareBootstrap::io_expander() noexcept { return mcp23017_; }
Ina226& HardwareBootstrap::battery_monitor() noexcept { return ina226_; }
Ds3231& HardwareBootstrap::rtc() noexcept { return ds3231_; }
SdStorage& HardwareBootstrap::storage() noexcept { return sd_storage_; }
W5500& HardwareBootstrap::ethernet() noexcept { return w5500_; }
OneWireRuntime& HardwareBootstrap::one_wire() noexcept { return one_wire_; }
Rs485Runtime& HardwareBootstrap::rs485() noexcept { return rs485_; }
Pzem004t& HardwareBootstrap::ac_meter() noexcept { return pzem004t_; }

}  // namespace homeguard::idf
