#pragma once

#include <cstdint>

using DWORD = std::uint32_t;
using FRESULT = int;

inline constexpr FRESULT FR_OK = 0;

struct FATFS {
    std::uint32_t csize = 0;
    std::uint32_t n_fatent = 0;
};

inline FRESULT f_getfree(
    const char*,
    DWORD* free_clusters,
    FATFS** filesystem)
{
    static FATFS mock_filesystem{};
    if (free_clusters != nullptr) {
        *free_clusters = 0;
    }
    if (filesystem != nullptr) {
        *filesystem = &mock_filesystem;
    }
    return FR_OK;
}
