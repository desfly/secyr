#include "homeguard/challenge.hpp"
namespace hg {
Challenge ChallengeManager::issue(CommandType command, uint64_t now, uint32_t ttl) { counter_=counter_*1664525U+1013904223U; current_={counter_,command,now+ttl,true}; return current_; }
bool ChallengeManager::consume(CommandType command, uint32_t token, uint64_t now) { const bool ok=current_.valid&&current_.command==command&&current_.token==token&&now<=current_.expires_at_ms; current_.valid=false; return ok; }
}
