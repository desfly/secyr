#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int64_t esp_timer_get_time(void) {
    // Deterministic monotonic stand-in for host syntax/link gates.
    static int64_t mock_time_us = 1000000;
    mock_time_us += 1000;
    return mock_time_us;
}

#ifdef __cplusplus
}
#endif
