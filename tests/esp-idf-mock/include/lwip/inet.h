#pragma once

#include "lwip/netif.h"
#include <arpa/inet.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline char* ip4addr_ntoa_r(const ip4_addr_t* addr, char* buffer, int length)
{
    if (!buffer || length <= 0) return 0;
    const uint32_t host = ntohl(addr ? addr->addr : 0);
    snprintf(buffer, (size_t)length, "%u.%u.%u.%u",
             (unsigned)((host >> 24) & 0xff),
             (unsigned)((host >> 16) & 0xff),
             (unsigned)((host >> 8) & 0xff),
             (unsigned)(host & 0xff));
    return buffer;
}

#ifdef __cplusplus
}
#endif
