#pragma once
#include <stdarg.h>

inline void esp_rom_delay_us(unsigned) {}
inline int esp_rom_vprintf(const char*, va_list) { return 0; }
