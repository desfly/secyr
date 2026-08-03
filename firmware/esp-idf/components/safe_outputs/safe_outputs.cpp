#include "safe_outputs.hpp"
#include "esp_log.h"

namespace { constexpr char tag[] = "hg_outputs"; }

bool SafeOutputs::begin() {
    all_off();
    ESP_LOGW(tag, "physical outputs disabled: GPIO numbers and active polarity are unverified");
    available_ = false;
    return false;
}

void SafeOutputs::apply(const hg::Outputs& outputs) {
    last_requested_ = outputs;
    if (!available_) all_off();
}

void SafeOutputs::all_off() {
    last_requested_ = {};
}
