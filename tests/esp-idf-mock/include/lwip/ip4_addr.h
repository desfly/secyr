#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

typedef struct ip4_addr {
    std::uint32_t addr;
} ip4_addr_t;

#ifndef IP4ADDR_STRLEN_MAX
#define IP4ADDR_STRLEN_MAX 16
#endif

inline char* ip4addr_ntoa_r(const ip4_addr_t* addr, char* buffer, int buflen)
{
    if (addr == nullptr || buffer == nullptr || buflen < 8) return nullptr;
    const auto value = addr->addr;
    const unsigned a = value & 0xffU;
    const unsigned b = (value >> 8U) & 0xffU;
    const unsigned c = (value >> 16U) & 0xffU;
    const unsigned d = (value >> 24U) & 0xffU;
    const int written = std::snprintf(buffer, static_cast<std::size_t>(buflen), "%u.%u.%u.%u", a, b, c, d);
    return written > 0 && written < buflen ? buffer : nullptr;
}
