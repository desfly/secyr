#include "homeguard/event_log.hpp"
#include <algorithm>
#include <stdexcept>
namespace hg {
void EventLog::append(uint64_t ts, Severity sev, uint16_t code, std::string_view text) {
 Event e{}; e.sequence=next_sequence_++; e.timestamp_ms=ts; e.severity=sev; e.code=code;
 const auto n=std::min(text.size(),e.text.size()-1); std::copy_n(text.begin(),n,e.text.begin()); e.text[n]='\0';
 events_[head_]=e; head_=(head_+1)%capacity; if(size_<capacity) ++size_;
}
const Event& EventLog::at_oldest(size_t index) const { if(index>=size_) throw std::out_of_range("event index"); const size_t oldest=(head_+capacity-size_)%capacity; return events_[(oldest+index)%capacity]; }
}
