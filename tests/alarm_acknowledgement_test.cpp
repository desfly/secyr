#include "alarm_acknowledgement.hpp"
#include <cassert>
#include <iostream>
using namespace homeguard;
int main(){
 AlarmAcknowledgementService s;
 assert(s.acknowledge(1,100,"android")==AlarmAckResult::NoActiveAlarm);
 s.on_alarm_state(true,42);
 assert(s.acknowledge(41,110,"android")==AlarmAckResult::InvalidRequest);
 assert(s.acknowledge(42,120,"android")==AlarmAckResult::Accepted);
 assert(s.acknowledge(42,130,"web")==AlarmAckResult::AlreadyAcknowledged);
 s.on_alarm_state(false,42); assert(!s.state().alarm_active);
 s.on_alarm_state(true,43); assert(!s.state().acknowledged && s.state().alarm_sequence==43);
 std::cout<<"alarm acknowledgement tests PASS\n";
}
