#include "homeguard/temperature_statistics.hpp"

#include <algorithm>
#include <numeric>

namespace homeguard {

TemperatureStatisticsFilter::TemperatureStatisticsFilter(
    std::size_t maximum_samples)
    : maximum_samples_(maximum_samples > 1 ? maximum_samples : 2)
{
}

TemperatureStatistics TemperatureStatisticsFilter::update(
    float temperature_c,
    std::uint64_t timestamp_ms,
    bool crc_ok)
{
    if (!crc_ok ||
        temperature_c < -55.0F ||
        temperature_c > 125.0F ||
        temperature_c == 85.0F) {
        statistics_.valid = false;
        return statistics_;
    }

    samples_.push_back({temperature_c, timestamp_ms});
    while (samples_.size() > maximum_samples_) {
        samples_.pop_front();
    }

    statistics_.valid = true;
    statistics_.latest_c = temperature_c;
    statistics_.samples = samples_.size();

    float sum = 0.0F;
    float minimum = samples_.front().value;
    float maximum = samples_.front().value;

    for (const auto& sample : samples_) {
        sum += sample.value;
        minimum = std::min(minimum, sample.value);
        maximum = std::max(maximum, sample.value);
    }

    statistics_.average_c =
        sum / static_cast<float>(samples_.size());
    statistics_.minimum_c = minimum;
    statistics_.maximum_c = maximum;
    statistics_.rate_c_per_minute = 0.0F;

    if (samples_.size() >= 2) {
        const auto& first = samples_.front();
        const auto& last = samples_.back();

        if (last.timestamp_ms > first.timestamp_ms) {
            const float minutes =
                static_cast<float>(
                    last.timestamp_ms -
                    first.timestamp_ms) / 60000.0F;
            statistics_.rate_c_per_minute =
                (last.value - first.value) / minutes;
        }
    }

    return statistics_;
}

const TemperatureStatistics&
TemperatureStatisticsFilter::statistics() const noexcept
{
    return statistics_;
}

}  // namespace homeguard
