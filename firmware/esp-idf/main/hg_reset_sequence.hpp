#pragma once

#include "esp_err.h"

namespace homeguard::idf {

// Starts the runtime factory-reset gesture on the dedicated service button:
// hold until RED, release; repeat three times; successful erase -> WHITE 5 s -> reboot.
esp_err_t start_service_button_factory_reset();

}  // namespace homeguard::idf
