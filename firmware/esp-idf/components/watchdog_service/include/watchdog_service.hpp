#pragma once
#include <cstdint>
class WatchdogService {
public:
    bool begin(uint32_t timeout_seconds = 8, bool panic_on_timeout = true);
    bool feed();
    void stop();
    [[nodiscard]] bool active() const { return active_; }
private:
    bool active_{};
};
