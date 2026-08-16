#pragma once

#include "homeguard/physical_output_runtime.hpp"
#include "homeguard/system_model.hpp"

#include "esp_err.h"

namespace homeguard::idf {

class OutputSupervisor {
public:
    esp_err_t start(
        hg::PhysicalOutputRuntime* runtime,
        const hg::SystemModel* model);

private:
    static void task_entry(void* context);
    void run();

    hg::PhysicalOutputRuntime* runtime_{};
    const hg::SystemModel* model_{};
};

}  // namespace homeguard::idf
