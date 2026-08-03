#pragma once

#include "esp_err.h"

namespace homeguard::idf {

class HardwareBootstrap;

class TelemetryRuntime {
public:
    esp_err_t start(HardwareBootstrap* hardware);

private:
    static void task_entry(void* context);
    void run();

    HardwareBootstrap* hardware_{nullptr};
};

}  // namespace homeguard::idf
