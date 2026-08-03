#pragma once
#include <cstdint>
namespace hg {
struct Outputs { bool siren{}; bool valve1{}; bool valve2{}; bool aux1{}; bool aux2{}; };
class MaintenanceGuard {
public:
 void enter(uint64_t now_ms); void exit();
 [[nodiscard]] bool active() const { return active_; }
 [[nodiscard]] uint64_t entered_at() const { return entered_at_; }
 Outputs apply(Outputs requested) const;
private: bool active_{}; uint64_t entered_at_{};
};
}
