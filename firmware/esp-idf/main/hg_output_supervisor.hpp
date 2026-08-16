#pragma once

#include "homeguard/boot_readiness.hpp"
#include "homeguard/physical_output_runtime.hpp"
#include "homeguard/system_model.hpp"

#include "esp_err.h"

namespace homeguard::idf {

class OutputSupervisor {
public:
    esp_err_t start(
        hg::PhysicalOutputRuntime* runtime,
        const hg::SystemModel* model,
        const hg::BootReadinessReport* readiness);

private:
    static void task_entry(void* context);
    void run();

    hg::PhysicalOutputRuntime* runtime_{};
    const hg::SystemModel* model_{};
    const hg::BootReadinessReport* readiness_{};
};

}  // namespace homeguard::idf
