#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>

namespace homeguard {

struct TemperatureStatistics {
    bool valid{false};
    float latest_c{0.0F};
    float average_c{0.0F};
    float minimum_c{0.0F};
    float maximum_c{0.0F};
    float rate_c_per_minute{0.0F};
    std::size_t samples{0};
};

class TemperatureStatisticsFilter {
public:
    explicit TemperatureStatisticsFilter(
        std::size_t maximum_samples = 60);

    TemperatureStatistics update(
        float temperature_c,
        std::uint64_t timestamp_ms,
        bool crc_ok);

    const TemperatureStatistics& statistics() const noexcept;

private:
    struct Sample {
        float value;
        std::uint64_t timestamp_ms;
    };

    std::size_t maximum_samples_;
    std::deque<Sample> samples_;
    TemperatureStatistics statistics_{};
};

}  // namespace homeguard
