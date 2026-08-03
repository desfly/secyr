#include "homeguard/maintenance.hpp"
namespace hg { void MaintenanceGuard::enter(uint64_t now){active_=true;entered_at_=now;} void MaintenanceGuard::exit(){active_=false;} Outputs MaintenanceGuard::apply(Outputs r)const{return active_?Outputs{}:r;} }
