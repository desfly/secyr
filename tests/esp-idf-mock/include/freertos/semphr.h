#pragma once
#include "freertos/FreeRTOS.h"

using SemaphoreHandle_t = void*;

#ifndef pdTRUE
#define pdTRUE 1
#endif
#ifndef pdFALSE
#define pdFALSE 0
#endif

inline SemaphoreHandle_t xSemaphoreCreateMutex(){return (void*)1;}
inline int xSemaphoreTake(SemaphoreHandle_t, TickType_t){return pdTRUE;}
inline int xSemaphoreGive(SemaphoreHandle_t){return pdTRUE;}
