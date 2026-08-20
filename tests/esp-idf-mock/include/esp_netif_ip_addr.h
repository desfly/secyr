#pragma once
#include <cstdint>
struct esp_ip4_addr_t { std::uint32_t addr{}; };
struct esp_netif_ip_info_t { esp_ip4_addr_t ip; };
struct ip_event_got_ip_t { esp_netif_ip_info_t ip_info; };
inline const char* esp_ip4addr_ntoa(const esp_ip4_addr_t*,char* out,int length){if(length>0){out[0]='0';if(length>1)out[1]=0;}return out;}
