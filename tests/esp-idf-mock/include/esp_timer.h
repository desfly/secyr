#pragma once

#include "esp_err.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*esp_timer_cb_t)(void* arg);

typedef enum {
    ESP_TIMER_TASK = 0,
    ESP_TIMER_ISR = 1,
} esp_timer_dispatch_t;

typedef struct mock_esp_timer {
    esp_timer_cb_t callback;
    void* arg;
    bool active;
} *esp_timer_handle_t;

typedef struct {
    esp_timer_cb_t callback;
    void* arg;
    esp_timer_dispatch_t dispatch_method;
    const char* name;
    bool skip_unhandled_events;
} esp_timer_create_args_t;

static inline int64_t esp_timer_get_time(void) {
    // Deterministic monotonic stand-in for host syntax/link gates.
    static int64_t mock_time_us = 1000000;
    mock_time_us += 1000;
    return mock_time_us;
}

static inline esp_err_t esp_timer_create(const esp_timer_create_args_t* args, esp_timer_handle_t* out) {
    static struct mock_esp_timer timer{};
    if (args == nullptr || args->callback == nullptr || out == nullptr) return ESP_ERR_INVALID_ARG;
    timer.callback = args->callback;
    timer.arg = args->arg;
    timer.active = false;
    *out = &timer;
    return ESP_OK;
}

static inline esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t) {
    if (timer == nullptr) return ESP_ERR_INVALID_ARG;
    timer->active = true;
    return ESP_OK;
}

static inline esp_err_t esp_timer_stop(esp_timer_handle_t timer) {
    if (timer == nullptr) return ESP_ERR_INVALID_ARG;
    timer->active = false;
    return ESP_OK;
}

static inline bool esp_timer_is_active(esp_timer_handle_t timer) {
    return timer != nullptr && timer->active;
}

static inline esp_err_t esp_timer_delete(esp_timer_handle_t timer) {
    if (timer == nullptr) return ESP_ERR_INVALID_ARG;
    timer->active = false;
    timer->callback = nullptr;
    timer->arg = nullptr;
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif
