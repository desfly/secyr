#include "w5500_adapter.hpp"
#include "esp_log.h"

namespace { constexpr char tag[] = "hg_w5500"; }

bool W5500Adapter::begin() {
    ESP_LOGW(tag, "W5500 adapter is unavailable until the verified SPI/GPIO map is supplied");
    initialized_ = false;
    link_up_ = false;
    return false;
}

bool W5500Adapter::link_up() const { return initialized_ && link_up_; }
