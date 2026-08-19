#pragma once
#include <cstdint>
using TickType_t = std::uint32_t;
#define pdMS_TO_TICKS(ms) (ms)
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
