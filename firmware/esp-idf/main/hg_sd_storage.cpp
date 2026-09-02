#include "hg_sd_storage.hpp"
#include "hg_board_hw678.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <cstdint>

#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "ff.h"
#include "sdmmc_cmd.h"

namespace homeguard::idf {

namespace {
constexpr const char* kTag = "hg_sd";
constexpr const char* kSelfTestFile = "/sdcard/HOMEGUARD.TXT";
constexpr const char* kSelfTestPayload =
    "HomeGuard-S3 microSD test OK\r\n"
    "WRITE: OK\r\n"
    "READ: OK\r\n";
}  // namespace

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

esp_err_t SdStorage::self_test()
{
    if (!status_.mounted || card_ == nullptr) {
        ESP_LOGE(kTag, "SD SELFTEST FAIL: card is not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    FILE* file = std::fopen(kSelfTestFile, "wb");
    if (file == nullptr) {
        ESP_LOGE(kTag, "SD WRITE FAIL: cannot create %s", kSelfTestFile);
        return ESP_FAIL;
    }

    const std::size_t payload_size = std::strlen(kSelfTestPayload);
    const std::size_t written =
        std::fwrite(kSelfTestPayload, 1, payload_size, file);
    const int flush_result = std::fflush(file);
    const int close_result = std::fclose(file);

    if (written != payload_size || flush_result != 0 || close_result != 0) {
        ESP_LOGE(kTag, "SD WRITE FAIL: wrote %u/%u bytes",
            static_cast<unsigned>(written),
            static_cast<unsigned>(payload_size));
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "SD WRITE OK: %s (%u bytes)",
        kSelfTestFile,
        static_cast<unsigned>(payload_size));

    file = std::fopen(kSelfTestFile, "rb");
    if (file == nullptr) {
        ESP_LOGE(kTag, "SD READ FAIL: cannot reopen %s", kSelfTestFile);
        return ESP_FAIL;
    }

    std::array<char, 128> buffer{};
    const std::size_t read =
        std::fread(buffer.data(), 1, payload_size, file);
    const int read_close_result = std::fclose(file);

    if (read != payload_size || read_close_result != 0 ||
        std::memcmp(buffer.data(), kSelfTestPayload, payload_size) != 0) {
        ESP_LOGE(kTag, "SD READ FAIL: data mismatch (%u/%u bytes)",
            static_cast<unsigned>(read),
            static_cast<unsigned>(payload_size));
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "SD READ OK: data verified");
    ESP_LOGI(kTag, "SD TEST OK: file kept on card as %s", kSelfTestFile);

    const auto space_error = refresh_space();
    if (space_error != ESP_OK) {
        ESP_LOGW(kTag, "SD free-space refresh failed after self-test: %s",
            esp_err_to_name(space_error));
    }
    return ESP_OK;
}

const SdStorageStatus& SdStorage::status() const noexcept
{
    return status_;
}

}  // namespace homeguard::idf
