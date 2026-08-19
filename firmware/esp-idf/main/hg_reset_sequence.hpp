#pragma once

#include "esp_err.h"

namespace homeguard::idf {

// Executes a previously staged factory reset during early boot, before network,
// HTTP, cloud, and other mutable-state users are started. Returns true only
// when a pending request took ownership of boot (the function then reboots).
bool handle_pending_factory_reset();

// Persistently stages a factory-reset request without erasing live NVS state.
// Caller may send its final response and then reboot. The next early boot owns
// the destructive erase and RED 5-second success indication.
esp_err_t stage_factory_reset_request();

// Starts the runtime gesture on the dedicated service button:
// hold until WHITE, release; repeat three times. The third confirmed release
// stages a reset request and reboots; early boot performs erase -> RED 5 s -> reboot.
esp_err_t start_service_button_factory_reset();

}  // namespace homeguard::idf
