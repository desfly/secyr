#include "homeguard/zone.hpp"
namespace hg {
ZoneState ZoneEvaluator::update(bool loop_closed, bool tamper, uint64_t now) {
 if(!cfg_.enabled) return state_=ZoneState::Disabled;
 ZoneState raw=tamper?ZoneState::Tamper:((loop_closed==cfg_.normally_closed)?ZoneState::Normal:ZoneState::Open);
 if(raw==state_) { candidate_=raw; candidate_since_=now; return state_; }
 if(raw!=candidate_) { candidate_=raw; candidate_since_=now; if(cfg_.debounce_ms==0) state_=candidate_; return state_; }
 if(now-candidate_since_>=cfg_.debounce_ms) state_=candidate_;
 return state_;
}
}
