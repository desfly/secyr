#pragma once

#include "lwip/netif.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int8_t s8_t;

struct eth_addr {
    uint8_t addr[6];
};

static inline s8_t etharp_find_addr(struct netif*, const ip4_addr_t*, struct eth_addr** eth_ret, const ip4_addr_t** ip_ret)
{
    if (eth_ret) *eth_ret = 0;
    if (ip_ret) *ip_ret = 0;
    return -1;
}

#ifdef __cplusplus
}
#endif
