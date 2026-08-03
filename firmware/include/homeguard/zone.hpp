#pragma once
#include "homeguard/config.hpp"
#include "homeguard/types.hpp"
#include <cstdint>
namespace hg {
class ZoneEvaluator {
public:
    ZoneEvaluator(ZoneConfig cfg={}) : cfg_(cfg) {}
    void configure(ZoneConfig cfg) { cfg_=cfg; }
    ZoneState update(bool loop_closed, bool tamper, uint64_t now_ms);
    [[nodiscard]] ZoneState state() const { return state_; }
private:
    ZoneConfig cfg_{}; ZoneState state_{ZoneState::Normal}; ZoneState candidate_{ZoneState::Normal}; uint64_t candidate_since_{};
};
}
