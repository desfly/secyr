#pragma once
#include "esp_eth.h"
struct eth_phy_config_t { int phy_addr; int reset_gpio_num; };
#define ETH_PHY_DEFAULT_CONFIG() eth_phy_config_t{}
inline esp_eth_phy_t* esp_eth_phy_new_w5500(const eth_phy_config_t*){static esp_eth_phy_t p;return &p;}
