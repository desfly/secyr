#pragma once

#include "homeguard/service_button.hpp"
#include <cstdint>

class PhysicalButtonService {
public:
    using Callback = void (*)(void* context);
    bool begin(int gpio, bool active_low, hg::ServiceButtonConfig config,
               Callback reboot_callback, Callback factory_reset_callback, void* context);
    void stop();
    [[nodiscard]] bool active() const { return running_; }
private:
    static void task_entry(void* context);
    void task();
    hg::ServiceButton button_{};
    Callback reboot_callback_{};
    Callback reset_callback_{};
    void* context_{};
    int gpio_{-1};
    bool active_low_{true};
    volatile bool running_{};
};
