#include "hg_sd_storage.hpp"
#include "hg_board_hw678.hpp"

#include <cstdint>

#include "driver/spi_common.h"
#include "esp_vfs_fat.h"
#include "ff.h"
#include "sdmmc_cmd.h"

namespace homeguard::idf {

esp_err_t SdStorage::mount()
{
    if (status_.mounted) {
        return ESP_OK;
    }

    const spi_bus_config_t bus_config{
        .mosi_io_num = board::kSdMosi,
        .miso_io_num = board::kSdMiso,
        .sclk_io_num = board::kSdSck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .max_transfer_sz = 4096,
        .flags = 0,
        .intr_flags = 0,
    };

    auto error = spi_bus_initialize(
        SPI3_HOST,
        &bus_config,
        SPI_DMA_CH_AUTO);
    if (error != ESP_OK &&
        error != ESP_ERR_INVALID_STATE) {
        return error;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;

    sdspi_device_config_t slot =
        SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = board::kSdCs;
    slot.host_id = SPI3_HOST;

    const esp_vfs_fat_sdmmc_mount_config_t mount_config{
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };

    error = esp_vfs_fat_sdspi_mount(
        status_.mount_point.c_str(),
        &host,
        &slot,
        &mount_config,
        &card_);

    if (error == ESP_OK) {
        status_.mounted = true;
        refresh_space();
    }
    return error;
}

esp_err_t SdStorage::unmount()
{
    if (!status_.mounted) {
        return ESP_OK;
    }

    const auto error =
        esp_vfs_fat_sdcard_unmount(
            status_.mount_point.c_str(),
            card_);

    if (error == ESP_OK) {
        status_ = {};
        status_.mount_point = "/sdcard";
        card_ = nullptr;
    }
    return error;
}

esp_err_t SdStorage::refresh_space()
{
    if (!status_.mounted || card_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    DWORD free_clusters = 0;
    FATFS* filesystem = nullptr;
    const auto result = f_getfree("0:", &free_clusters, &filesystem);
    if (result != FR_OK || filesystem == nullptr) {
        return ESP_FAIL;
    }

    const auto sector_size =
        static_cast<std::uint64_t>(card_->csd.sector_size);
    const auto sectors_per_cluster =
        static_cast<std::uint64_t>(filesystem->csize);
    const auto total_clusters =
        static_cast<std::uint64_t>(filesystem->n_fatent - 2U);

    status_.total_bytes =
        total_clusters * sectors_per_cluster * sector_size;
    status_.free_bytes =
        static_cast<std::uint64_t>(free_clusters) *
        sectors_per_cluster * sector_size;
    return ESP_OK;
}

const SdStorageStatus& SdStorage::status() const noexcept
{
    return status_;
}

}  // namespace homeguard::idf
