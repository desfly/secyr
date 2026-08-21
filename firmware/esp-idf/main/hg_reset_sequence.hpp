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

// Physical HomeGuard-S3 RST/EN sequence. This board reports its hardware RST
// button as POWERON; RTC state distinguishes it from a true cold boot. Each
// accepted step gets WHITE acknowledgement. The third stages Factory Reset;
// successful erase is confirmed by RED for 5 seconds.
bool handle_physical_rst_factory_reset();

}  // namespace homeguard::idf
