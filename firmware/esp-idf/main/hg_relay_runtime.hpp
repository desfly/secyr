#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <cstdint>

namespace homeguard::idf {

class Mcp23017;
class ZoneMonitor;

struct RelayRuntimeState {
    bool light_active{};
    bool light_manual{};
    bool light_automatic{};
    bool lock_active{};
    std::uint32_t lock_remaining_ms{};
};

class RelayRuntime {
public:
    static constexpr std::uint8_t kLightMask = 1U << 0U;  // MCP23017 GPA0
    static constexpr std::uint8_t kLockMask = 1U << 6U;   // MCP23017 GPA6
    static constexpr std::uint32_t kLightCycleMs = 60'000U;
    static constexpr std::uint32_t kLockPulseMs = 5'000U;

    esp_err_t start(Mcp23017* expander, ZoneMonitor* zones);
    bool set_manual_light(bool active);
    bool request_lock_pulse();
    RelayRuntimeState state();

private:
    static void task_entry(void* context);
    void run();
    bool write_state_locked(bool light, bool lock);
    static std::uint64_t now_ms();

    Mcp23017* expander_{nullptr};
    ZoneMonitor* zones_{nullptr};
    SemaphoreHandle_t mutex_{nullptr};
    bool light_active_{false};
    bool manual_light_{false};
    bool automatic_light_{false};
    bool lock_active_{false};
    std::uint64_t light_deadline_ms_{0};
    std::uint64_t lock_deadline_ms_{0};
};

}  // namespace homeguard::idf
