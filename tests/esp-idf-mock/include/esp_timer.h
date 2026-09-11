#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_timer_mock* esp_timer_handle_t;
typedef void (*esp_timer_cb_t)(void* arg);

typedef struct {
    esp_timer_cb_t callback;
    void* arg;
    int dispatch_method;
    const char* name;
    bool skip_unhandled_events;
} esp_timer_create_args_t;

static inline int64_t esp_timer_get_time(void) {
    // Deterministic monotonic stand-in for host syntax/link gates.
    static int64_t mock_time_us = 1000000;
    mock_time_us += 1000;
    return mock_time_us;
}

static inline esp_err_t esp_timer_create(const esp_timer_create_args_t* args, esp_timer_handle_t* out_handle) {
    if (args == 0 || out_handle == 0 || args->callback == 0) return ESP_ERR_INVALID_ARG;
    *out_handle = (esp_timer_handle_t)args;
    return ESP_OK;
}

static inline esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us) {
    return (timer != 0 && period_us != 0) ? ESP_OK : ESP_ERR_INVALID_ARG;
}

static inline bool esp_timer_is_active(esp_timer_handle_t timer) {
    return timer != 0;
}

static inline esp_err_t esp_timer_stop(esp_timer_handle_t timer) {
    return timer != 0 ? ESP_OK : ESP_ERR_INVALID_ARG;
}

static inline esp_err_t esp_timer_delete(esp_timer_handle_t timer) {
    return timer != 0 ? ESP_OK : ESP_ERR_INVALID_ARG;
}

#ifdef __cplusplus
}
#endif
