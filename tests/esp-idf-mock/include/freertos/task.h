#pragma once
#include "freertos/FreeRTOS.h"
using TaskFunction_t = void(*)(void*);
using TaskHandle_t = void*;
inline int xTaskCreate(TaskFunction_t, const char*, unsigned, void*, unsigned, TaskHandle_t*) { return pdPASS; }
inline TickType_t xTaskGetTickCount() { static TickType_t tick = 0; return ++tick; }
inline void vTaskDelay(TickType_t) {}
inline void vTaskDelete(TaskHandle_t) {}
