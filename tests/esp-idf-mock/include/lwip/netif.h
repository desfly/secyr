#pragma once

#include <stdint.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ip4_addr {
    uint32_t addr;
} ip4_addr_t;

struct netif {
    ip4_addr_t ip_addr;
};

extern struct netif* netif_default;

static inline const ip4_addr_t* netif_ip4_addr(const struct netif* n) { return n ? &n->ip_addr : 0; }
static inline uint32_t ip4_addr_get_u32(const ip4_addr_t* a) { return a ? a->addr : 0; }
static inline void ip4_addr_set_u32(ip4_addr_t* a, uint32_t v) { if (a) a->addr = v; }
static inline int ip4_addr_isany_val(ip4_addr_t a) { return a.addr == 0; }

#ifndef PP_HTONL
#define PP_HTONL(x) htonl((uint32_t)(x))
#endif

#ifdef __cplusplus
}
#endif
