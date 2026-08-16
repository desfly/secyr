#pragma once
#include <stdarg.h>

using vprintf_like_t = int (*)(const char*, va_list);
inline vprintf_like_t esp_log_set_vprintf(vprintf_like_t func) { return func; }

#define ESP_LOGD(tag, fmt, ...) do {} while(0)
#define ESP_LOGI(tag, fmt, ...) do {} while(0)
#define ESP_LOGE(tag, fmt, ...) do {} while(0)
#define ESP_LOGW(tag, fmt, ...) do {} while(0)
