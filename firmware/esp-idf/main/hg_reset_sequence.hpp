#pragma once

#include "esp_err.h"

namespace homeguard::idf {

// Executes a previously staged factory reset during early boot, before network,
// HTTP, cloud, and other mutable-state users are started. Successful erase is
// confirmed by RED for 5 seconds, then the controller reboots.
bool handle_pending_factory_reset();

// Persistently stages a factory-reset request without erasing live NVS state.
// Caller may send its final response and then reboot; early boot owns erase.
esp_err_t stage_factory_reset_request();

// Compatibility entry point used by app_main. Despite the historical name,
// this now restores the physical RST/EN boot gesture; GPIO21 is not read.
// Three accepted physical RST steps receive WHITE acknowledgement; the third
// stages Factory Reset, whose successful completion is RED for 5 seconds.
esp_err_t start_service_button_factory_reset();

}  // namespace homeguard::idf
