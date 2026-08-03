#include "test_framework.hpp"
#include "homeguard/zone.hpp"
void test_zone(){
 hg::ZoneEvaluator z({true,true,100});
 CHECK(z.state()==hg::ZoneState::Normal);
 CHECK(z.update(true,false,0)==hg::ZoneState::Normal);
 CHECK(z.update(false,false,10)==hg::ZoneState::Normal);
 CHECK(z.update(false,false,109)==hg::ZoneState::Normal);
 CHECK(z.update(false,false,110)==hg::ZoneState::Open);
 CHECK(z.update(true,false,150)==hg::ZoneState::Open);
 CHECK(z.update(true,false,250)==hg::ZoneState::Normal);
 CHECK(z.update(true,true,300)==hg::ZoneState::Normal);
 CHECK(z.update(true,true,400)==hg::ZoneState::Tamper);
 z.configure({false,true,10}); CHECK(z.update(false,false,500)==hg::ZoneState::Disabled);
}
