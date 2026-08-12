#pragma once

#include "lwip/ip4_addr.h"

#include <cstddef>
#include <cstdint>

#ifndef ETHARP_TABLE_SIZE
#define ETHARP_TABLE_SIZE 4
#endif

struct netif {};
struct eth_addr { std::uint8_t addr[6]; };

inline int etharp_get_entry(std::size_t,
                            ip4_addr_t** ip,
                            struct netif** interface,
                            struct eth_addr** mac)
{
    if (ip != nullptr) *ip = nullptr;
    if (interface != nullptr) *interface = nullptr;
    if (mac != nullptr) *mac = nullptr;
    return 0;
}
