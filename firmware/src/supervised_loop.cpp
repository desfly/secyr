#include "homeguard/supervised_loop.hpp"

namespace homeguard {

SupervisedLoopFilter::SupervisedLoopFilter(
    SupervisedLoopThresholds thresholds,
    float alpha,
    std::uint32_t required_stable_samples)
    : thresholds_(thresholds),
      alpha_(alpha > 0.0F && alpha <= 1.0F ? alpha : 0.25F),
      required_stable_samples_(
          required_stable_samples > 0 ? required_stable_samples : 1)
{
}

SupervisedLoopState SupervisedLoopFilter::classify(
    float millivolts) const noexcept
{
    if (millivolts < 0.0F) {
        return SupervisedLoopState::SensorFault;
    }
    if (millivolts <= thresholds_.short_max_mv) {
        return SupervisedLoopState::ShortCircuit;
    }
    if (millivolts >= thresholds_.normal_min_mv &&
        millivolts < thresholds_.normal_max_mv) {
        return SupervisedLoopState::Normal;
    }
    if (millivolts >= thresholds_.alarm_min_mv &&
        millivolts <= thresholds_.alarm_max_mv) {
        return SupervisedLoopState::Alarm;
    }
    if (millivolts >= thresholds_.open_min_mv) {
        return SupervisedLoopState::OpenCircuit;
    }
    return SupervisedLoopState::Unstable;
}

SupervisedLoopReading SupervisedLoopFilter::update(float millivolts)
{
    reading_.raw_mv = millivolts;

    if (!initialized_) {
        initialized_ = true;
        reading_.filtered_mv = millivolts;
    } else {
        reading_.filtered_mv =
            reading_.filtered_mv * (1.0F - alpha_) +
            millivolts * alpha_;
    }

    const auto next = classify(reading_.filtered_mv);
    if (next == candidate_) {
        ++candidate_samples_;
    } else {
        candidate_ = next;
        candidate_samples_ = 1;
    }

    if (candidate_samples_ >= required_stable_samples_ &&
        reading_.state != candidate_) {
        reading_.state = candidate_;
        reading_.stable_samples = candidate_samples_;
        ++reading_.transition_count;
    } else {
        reading_.stable_samples = candidate_samples_;
    }

    return reading_;
}

const SupervisedLoopReading& SupervisedLoopFilter::reading() const noexcept
{
    return reading_;
}

const char* to_string(SupervisedLoopState state) noexcept
{
    switch (state) {
    case SupervisedLoopState::Normal:
        return "normal";
    case SupervisedLoopState::Alarm:
        return "alarm";
    case SupervisedLoopState::ShortCircuit:
        return "short";
    case SupervisedLoopState::OpenCircuit:
        return "open";
    case SupervisedLoopState::Unstable:
        return "unstable";
    case SupervisedLoopState::SensorFault:
    default:
        return "fault";
    }
}

}  // namespace homeguard
