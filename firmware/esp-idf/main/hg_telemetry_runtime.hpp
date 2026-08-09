#pragma once

#include "esp_err.h"

namespace hg { class SystemModel; }
namespace homeguard::idf {

class CloudLink;
class HardwareBootstrap;

class TelemetryRuntime {
public:
    esp_err_t start(HardwareBootstrap* hardware, hg::SystemModel* model, CloudLink* cloud);
    esp_err_t publish_now();

private:
    static void task_entry(void* context);
    void run();

    HardwareBootstrap* hardware_{nullptr};
    hg::SystemModel* model_{nullptr};
    CloudLink* cloud_{nullptr};
    unsigned long long sequence_{0};
};

}  // namespace homeguard::idf
