#pragma once

#include "homeguard/hardware_runtime.hpp"
#include "hg_ads1115.hpp"
#include "hg_i2c_bus.hpp"
#include "hg_mcp23017.hpp"
#include "hg_w5500.hpp"
#include "hg_rs485.hpp"
#include "hg_onewire_runtime.hpp"
#include "hg_sd_storage.hpp"
#include "hg_ds3231.hpp"
#include "hg_ina226.hpp"

#include <string>

namespace homeguard::idf {

class HardwareBootstrap {
public:
    esp_err_t initialize();
    const HardwareRuntimeStatus& status() const noexcept;
    std::string analog_snapshot_json();

    Ads1115& zone_adc() noexcept;
    Ads1115& telemetry_adc() noexcept;
    Mcp23017& io_expander() noexcept;
    Ina226& battery_monitor() noexcept;
    Ds3231& rtc() noexcept;
    SdStorage& storage() noexcept;
    W5500& ethernet() noexcept;
    OneWireRuntime& one_wire() noexcept;
    Rs485Runtime& rs485() noexcept;

private:
    static HardwareModuleStatus module_status(
        esp_err_t error,
        const char* ready_detail,
        const char* failure_detail);

    HardwareRuntimeStatus status_{};
    I2cBus i2c_;
    Ads1115 zone_adc_;
    Ads1115 telemetry_adc_;
    Mcp23017 mcp23017_;
    Ina226 ina226_;
    Ds3231 ds3231_;
    SdStorage sd_storage_;
    W5500 w5500_;
    OneWireRuntime one_wire_;
    Rs485Runtime rs485_;
};

}  // namespace homeguard::idf
