#pragma once

#include "esp_err.h"

namespace homeguard::idf {

// Enter a fail-closed recovery loop when ordinary NVS initialization fails.
// This function does not start network/HTTP/cloud. Only the physical service
// button gesture may authorize a full NVS erase.
[[noreturn]] void enter_nvs_recovery_mode(esp_err_t init_error);

}  // namespace homeguard::idf
