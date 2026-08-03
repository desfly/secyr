#pragma once
#include "esp_err.h"
#define ESP_RETURN_ON_ERROR(expr, tag, msg) do { auto _e=(expr); if(_e!=ESP_OK) return _e; } while(0)
#define ESP_ERROR_CHECK(expr) do { (void)(expr); } while(0)
