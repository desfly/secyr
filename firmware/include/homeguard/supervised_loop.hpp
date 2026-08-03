#pragma once

#include <cstddef>
#include <cstdint>

namespace homeguard {

enum class SupervisedLoopState {
    Normal,
    Alarm,
    ShortCircuit,
    OpenCircuit,
    Unstable,
    SensorFault,
};

struct SupervisedLoopThresholds {
    float short_max_mv{350.0F};
    float normal_min_mv{1050.0F};
    float normal_max_mv{1900.0F};
    float alarm_min_mv{1900.0F};
    float alarm_max_mv{2750.0F};
    float open_min_mv{2950.0F};
};

struct SupervisedLoopReading {
    float raw_mv{0.0F};
    float filtered_mv{0.0F};
    SupervisedLoopState state{SupervisedLoopState::SensorFault};
    std::uint32_t stable_samples{0};
    std::uint32_t transition_count{0};
};

class SupervisedLoopFilter {
public:
    SupervisedLoopFilter(
        SupervisedLoopThresholds thresholds = {},
        float alpha = 0.25F,
        std::uint32_t required_stable_samples = 4);

    SupervisedLoopReading update(float millivolts);
    const SupervisedLoopReading& reading() const noexcept;

private:
    SupervisedLoopState classify(float millivolts) const noexcept;

    SupervisedLoopThresholds thresholds_;
    float alpha_;
    std::uint32_t required_stable_samples_;
    bool initialized_{false};
    SupervisedLoopState candidate_{SupervisedLoopState::SensorFault};
    std::uint32_t candidate_samples_{0};
    SupervisedLoopReading reading_{};
};

const char* to_string(SupervisedLoopState state) noexcept;

}  // namespace homeguard
