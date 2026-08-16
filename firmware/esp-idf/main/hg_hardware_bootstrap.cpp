#include "hg_hardware_bootstrap.hpp"

namespace homeguard::idf {

HardwareModuleStatus HardwareBootstrap::module_status(
    esp_err_t error,
    const char* ready_detail,
    const char* failure_detail)
{
    if (error == ESP_OK) {
        return {HardwareModuleState::Ready, ready_detail, 0};
    }

    const auto state =
        (error == ESP_ERR_NOT_FOUND || error == ESP_ERR_TIMEOUT || error == ESP_ERR_INVALID_RESPONSE)
            ? HardwareModuleState::Missing
            : HardwareModuleState::Fault;
    return {state, failure_detail, 1};
}

esp_err_t HardwareBootstrap::initialize()
{
    status_ = {};
    status_.overall = HardwareBootstrapState::NotInitialized;
    status_.safe_outputs_forced = false;

    auto error = i2c_.initialize();
    status_.i2c = module_status(
        error,
        "GPIO4/GPIO5, 400 kHz devices",
        "I2C bus initialization failed");

    if (error != ESP_OK) {
        // The shared I2C infrastructure is a platform-level dependency for the
        // HW-678 sensor/output architecture. Optional module absence is merely
        // degraded, but failure to create the bus itself is a failed bootstrap.
        status_.overall = HardwareBootstrapState::Failed;
        return error;
    }

    const auto zone_error = zone_adc_.initialize(i2c_, 0x48);
    status_.ads1115_zones = module_status(
        zone_error,
        "ADS1115 0x48",
        "ADS1115 0x48 not detected");

    const auto telemetry_error = telemetry_adc_.initialize(i2c_, 0x49);
    status_.ads1115_telemetry = module_status(
        telemetry_error,
        "ADS1115 0x49",
        "ADS1115 0x49 not detected");

    const auto mcp_error = mcp23017_.initialize(i2c_, 0x20);
    status_.mcp23017 = module_status(
        mcp_error,
        "MCP23017 0x20, outputs forced OFF",
        "MCP23017 0x20 not detected");

    if (mcp_error == ESP_OK) {
        const auto safe_error = mcp23017_.force_safe_outputs();
        status_.safe_outputs_forced = safe_error == ESP_OK;
        if (safe_error != ESP_OK) {
            status_.mcp23017 = {
                HardwareModuleState::Fault,
                "MCP23017 detected but safe OFF latch could not be confirmed",
                1,
            };
        }
    }

    const auto ina_error = ina226_.initialize(i2c_, 0x40, 0.010F, 20.0F);
    status_.ina226 = module_status(
        ina_error,
        "INA226 0x40, shunt 10 mOhm",
        "INA226 0x40 not detected");

    const auto rtc_error = ds3231_.initialize(i2c_, 0x68);
    status_.ds3231 = module_status(
        rtc_error,
        "DS3231 0x68",
        "DS3231 0x68 not detected");

    const auto sd_error = sd_storage_.mount();
    status_.micro_sd = module_status(
        sd_error,
        "microSD mounted at /sdcard",
        "microSD not available");

    const auto eth_error = w5500_.initialize();
    status_.w5500 = module_status(
        eth_error,
        "W5500 initialized",
        "W5500 initialization failed");

    if (eth_error == ESP_OK) {
        const auto start_error = w5500_.start();
        if (start_error != ESP_OK) {
            status_.w5500 = {
                HardwareModuleState::Fault,
                "W5500 initialized but start failed; driver rolled back",
                1,
            };
        }
    }

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

    status_.overall = unavailable_count() == 0U
        ? HardwareBootstrapState::Ready
        : HardwareBootstrapState::Degraded;
    return ESP_OK;
}

const HardwareRuntimeStatus& HardwareBootstrap::status() const noexcept
{
    return status_;
}

std::uint32_t HardwareBootstrap::unavailable_count() const noexcept
{
    const HardwareModuleStatus* modules[] = {
        &status_.ads1115_zones,
        &status_.ads1115_telemetry,
        &status_.mcp23017,
        &status_.ina226,
        &status_.ds3231,
        &status_.w5500,
        &status_.micro_sd,
        &status_.one_wire,
        &status_.rs485,
    };

    std::uint32_t count = 0;
    for (const auto* module : modules) {
        if (module->state != HardwareModuleState::Ready) ++count;
    }
    return count;
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

}  // namespace homeguard::idf
