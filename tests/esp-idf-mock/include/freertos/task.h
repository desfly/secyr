#pragma once
#include "freertos/FreeRTOS.h"
using TaskFunction_t = void(*)(void*);
inline int xTaskCreate(TaskFunction_t, const char*, unsigned, void*, unsigned, void*) { return pdPASS; }
inline void vTaskDelay(TickType_t) {}
