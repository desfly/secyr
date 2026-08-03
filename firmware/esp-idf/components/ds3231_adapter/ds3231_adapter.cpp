#include "ds3231_adapter.hpp"
#include "esp_log.h"

namespace { constexpr char tag[] = "hg_ds3231"; }

bool Ds3231Adapter::begin() {
    ESP_LOGW(tag, "DS3231 adapter unavailable: I2C pins are unverified");
    available_ = false;
    return false;
}

uint64_t Ds3231Adapter::epoch() const { return 0; }
