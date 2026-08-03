#include "gpio_inputs.hpp"
#include "esp_log.h"

namespace { constexpr char tag[] = "hg_inputs"; }

bool GpioInputs::begin() {
    ESP_LOGW(tag, "alarm GPIO inputs unavailable: zone pin map is not verified");
    available_ = false;
    return false;
}

bool GpioInputs::loop_closed(uint8_t zone) const {
    static_cast<void>(zone);
    return false;
}

bool GpioInputs::tamper(uint8_t zone) const {
    static_cast<void>(zone);
    return true;
}
