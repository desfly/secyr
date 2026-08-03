#pragma once
#include "esp_err.h"
#include "sdmmc_cmd.h"
struct esp_vfs_fat_sdmmc_mount_config_t {
    bool format_if_mount_failed; int max_files; int allocation_unit_size;
    bool disk_status_check_enable; bool use_one_fat;
};
inline esp_err_t esp_vfs_fat_sdspi_mount(const char*,const sdmmc_host_t*,const sdspi_device_config_t*,const esp_vfs_fat_sdmmc_mount_config_t*,sdmmc_card_t** c){static sdmmc_card_t x;*c=&x;return ESP_OK;}
inline esp_err_t esp_vfs_fat_sdcard_unmount(const char*,sdmmc_card_t*){return ESP_OK;}
