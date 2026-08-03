#pragma once
#include "homeguard/types.hpp"
#include <cstdint>
namespace hg {
struct LinkInputs { bool ethernet{}; bool wifi_sta{}; bool ap_ready{true}; };
class NetworkFailover {
public:
 NetworkFailover(uint32_t debounce_ms, uint32_t hold_ms) : debounce_ms_(debounce_ms), hold_ms_(hold_ms) {}
 Transport update(LinkInputs links, uint64_t now_ms);
 [[nodiscard]] Transport active() const { return active_; }
private:
 Transport preferred(LinkInputs links) const;
 uint32_t debounce_ms_{}; uint32_t hold_ms_{}; Transport active_{Transport::None}; Transport candidate_{Transport::None}; uint64_t candidate_since_{}; uint64_t active_since_{};
};
}
