#include "hg_sd_storage.hpp"
#include "hg_board_hw678.hpp"
#include <cstdint>

#include "driver/spi_common.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sys/statvfs.h"

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
    if (!status_.mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    struct statvfs info {};
    if (statvfs(
            status_.mount_point.c_str(),
            &info) != 0) {
        return ESP_FAIL;
    }

    status_.total_bytes =
        static_cast<std::uint64_t>(
            info.f_blocks) * info.f_frsize;
    status_.free_bytes =
        static_cast<std::uint64_t>(
            info.f_bavail) * info.f_frsize;
    return ESP_OK;
}

const SdStorageStatus& SdStorage::status() const noexcept
{
    return status_;
}

}  // namespace homeguard::idf
