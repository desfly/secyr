#pragma once
#include "esp_eth.h"
#include "driver/spi_master.h"
struct eth_w5500_config_t { int int_gpio_num; int poll_period_ms; };
#define ETH_W5500_DEFAULT_CONFIG(host,cfg) eth_w5500_config_t{}
struct eth_mac_config_t {};
#define ETH_MAC_DEFAULT_CONFIG() eth_mac_config_t{}
inline esp_eth_mac_t* esp_eth_mac_new_w5500(const eth_w5500_config_t*,const eth_mac_config_t*){static esp_eth_mac_t m;return &m;}
