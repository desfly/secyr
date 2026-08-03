#pragma once
#include "homeguard/types.hpp"
#include <cstdint>
namespace hg {
struct Challenge { uint32_t token{}; CommandType command{}; uint64_t expires_at_ms{}; bool valid{}; };
class ChallengeManager {
public:
 Challenge issue(CommandType command, uint64_t now_ms, uint32_t ttl_ms);
 bool consume(CommandType command, uint32_t token, uint64_t now_ms);
private: Challenge current_{}; uint32_t counter_{0x31534748U};
};
}
