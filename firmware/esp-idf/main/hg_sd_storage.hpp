#pragma once

#include "esp_err.h"
#include "sdmmc_cmd.h"

#include <cstdint>
#include <string>

namespace homeguard::idf {

struct SdStorageStatus {
    bool mounted{false};
    std::uint64_t total_bytes{0};
    std::uint64_t free_bytes{0};
    std::string mount_point{"/sdcard"};
};

class SdStorage {
public:
    esp_err_t mount();
    esp_err_t unmount();
    esp_err_t refresh_space();
    const SdStorageStatus& status() const noexcept;

private:
    sdmmc_card_t* card_{nullptr};
    bool spi_bus_owned_{false};
    SdStorageStatus status_{};
};

}  // namespace homeguard::idf
