#include "hg_cloud_trusted_time.hpp"

#include "esp_netif_sntp.h"

#include <ctime>
#include <cstdint>

namespace homeguard::idf {
namespace {
constexpr std::time_t kMinimumTrustedEpoch = 1735689600; // 2025-01-01 UTC
}

esp_err_t CloudTrustedTime::start()
{
    const esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    return esp_netif_sntp_init(&config);
}

bool CloudTrustedTime::ready() const
{
    std::time_t now = 0;
    std::time(&now);
    return now >= kMinimumTrustedEpoch;
}

std::uint64_t CloudTrustedTime::now_ms() const
{
    if (!ready()) return 0;
    struct timespec ts {};
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return 0;
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000ULL +
           static_cast<std::uint64_t>(ts.tv_nsec / 1000000L);
}

}  // namespace homeguard::idf
