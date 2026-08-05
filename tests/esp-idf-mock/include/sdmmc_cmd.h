#pragma once
#include "esp_err.h"

struct sdmmc_csd_t {
    unsigned sector_size;
};

struct sdmmc_card_t {
    sdmmc_csd_t csd;
};

struct sdmmc_host_t {
    int slot;
};

#define SDSPI_HOST_DEFAULT() sdmmc_host_t{}

struct sdspi_device_config_t {
    int gpio_cs;
    int host_id;
};

#define SDSPI_DEVICE_CONFIG_DEFAULT() sdspi_device_config_t{}
