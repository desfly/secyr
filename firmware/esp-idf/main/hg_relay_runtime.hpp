#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <cstdint>

namespace homeguard::idf {

class ZoneMonitor;

struct RelayRuntimeState {
    bool light_active{};
    bool light_manual{};
    bool light_automatic{};
    bool lock_active{};
    bool valve1_active{};
    bool valve2_active{};
    std::uint32_t lock_remaining_ms{};
};

class RelayRuntime {
public:
    static constexpr std::uint32_t kLightCycleMs = 60'000U;
    static constexpr std::uint32_t kLockPulseMs = 5'000U;

    esp_err_t start(ZoneMonitor* zones);
    bool set_manual_light(bool active);
    bool request_lock_pulse();
    bool set_valve(std::uint8_t index, bool active);
    RelayRuntimeState state();

private:
    static void task_entry(void* context);
    void run();
    bool write_state_locked(bool light, bool lock, bool valve1, bool valve2);
    bool force_safe_locked();
    static std::uint64_t now_ms();

    ZoneMonitor* zones_{nullptr};
    SemaphoreHandle_t mutex_{nullptr};
    bool light_active_{false};
    bool manual_light_{false};
    bool automatic_light_{false};
    bool lock_active_{false};
    bool valve1_active_{false};
    bool valve2_active_{false};
    std::uint64_t light_deadline_ms_{0};
    std::uint64_t lock_deadline_ms_{0};
};

}  // namespace homeguard::idf
