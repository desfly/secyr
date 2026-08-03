#include "homeguard/idempotency.hpp"
namespace hg {
bool IdempotencyCache::accept(uint64_t id, uint64_t issued, uint64_t now, uint32_t ttl) {
 if(id==0||issued>now||now-issued>ttl) return false;
 for(const auto& e:entries_) if(e.used&&e.id==id&&now-e.seen_at<=ttl) return false;
 entries_[cursor_]={id,now,true}; cursor_=(cursor_+1)%capacity; if(size_<capacity) ++size_; return true;
}
}
