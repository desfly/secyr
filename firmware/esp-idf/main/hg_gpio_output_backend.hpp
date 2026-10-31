#pragma once

#include "homeguard/physical_output_runtime.hpp"
#include "hg_mcp23017.hpp"
#include <cstdint>
#include <mutex>

namespace homeguard::idf {

class GpioOutputBackend final : public hg::PhysicalOutputBackend {
public:
    using Clock = std::uint64_t (*)();
    explicit GpioOutputBackend(Clock clock = nullptr) : clock_(clock) {}
    void bind_expander(Mcp23017& expander) noexcept { expander_ = &expander; }
    bool configure_output(int gpio, bool initial_level) override;
    bool write_output(int gpio, bool level) override;
    bool synchronize_relays(const hg::SystemModel& model) override;
    bool force_safe_relays() override;
    bool enforce_lock_timeout(hg::SystemModel& model);
    std::uint8_t relay_levels();
private:
    bool write_expander(int gpio, bool level);
    bool apply_expander(std::uint8_t next);
    std::uint64_t now_ms() const;
    Clock clock_;
    std::uint64_t lock_deadline_ms_{0};
    Mcp23017* expander_{nullptr};
    std::mutex expander_mutex_;
    std::uint8_t expander_shadow_{0};
    bool expander_fault_{false};
};

}  // namespace homeguard::idf
