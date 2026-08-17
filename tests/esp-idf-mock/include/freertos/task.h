#pragma once
#include "freertos/FreeRTOS.h"

#include <cstdint>

using TaskFunction_t = void(*)(void*);
using TaskHandle_t = void*;

inline int xTaskCreate(TaskFunction_t, const char*, unsigned, void*, unsigned, void*) { return pdPASS; }
inline TaskHandle_t xTaskGetCurrentTaskHandle() { return reinterpret_cast<TaskHandle_t>(1); }
inline int xTaskNotifyGive(TaskHandle_t) { return pdPASS; }
inline std::uint32_t ulTaskNotifyTake(int, TickType_t) { return 1U; }
inline void vTaskDelay(TickType_t) {}
inline void vTaskDelete(void*) {}
