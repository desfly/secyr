#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int64_t esp_timer_get_time(void)
{
    static int64_t now_us = 1000000;
    now_us += 1000;
    return now_us;
}

#ifdef __cplusplus
}
#endif
