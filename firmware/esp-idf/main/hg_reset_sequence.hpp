#pragma once

#include "esp_err.h"

namespace homeguard::idf {

// Executes a previously staged reset during early boot, before network, HTTP,
// cloud, and other mutable-state users are started. The public name is retained
// for compatibility with existing callers; it now handles both settings-only
// and full factory requests.
bool handle_pending_factory_reset();

// Persistently stages a full factory-reset request without erasing live NVS
// state. HTTP and the 5-step physical path use the same safe early-boot erase.
esp_err_t stage_factory_reset_request();

// Physical HomeGuard-S3 RST/EN sequence. HW-678 reports its hardware RST/EN
// button as POWERON, so a persistent NVS boot baseline is used. Every accepted
// step gets WHITE acknowledgement. Three steps with no continuation stage a
// settings-only reset that preserves users; five steps select full factory
// reset. Successful settings reset is WHITE for 5 s; full factory is RED 5 s.
bool handle_physical_rst_factory_reset();

// Legacy app_main entry point retained only to keep the change isolated. It no
// longer reads GPIO21 or starts a service-button task; it delegates entirely to
// the physical RST/EN boot detector above.
inline esp_err_t start_service_button_factory_reset() {
    (void)handle_physical_rst_factory_reset();
    return ESP_OK;
}

}  // namespace homeguard::idf
