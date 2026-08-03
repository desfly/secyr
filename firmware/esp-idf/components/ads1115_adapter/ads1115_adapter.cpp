#include "ads1115_adapter.hpp"
#include "esp_log.h"
#include <limits>

namespace { constexpr char tag[] = "hg_ads1115"; }

bool Ads1115Adapter::begin(uint8_t address) {
    static_cast<void>(address);
    ESP_LOGW(tag, "ADS1115 adapter unavailable: I2C pins and electrical calibration are unverified");
    available_ = false;
    return false;
}

float Ads1115Adapter::read_pressure(uint8_t channel) {
    static_cast<void>(channel);
    return std::numeric_limits<float>::quiet_NaN();
}
