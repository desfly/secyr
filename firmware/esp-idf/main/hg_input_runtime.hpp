#pragma once

#include "homeguard/system_model.hpp"
#include "esp_err.h"

namespace homeguard::idf {

class InputRuntime {
public:
    esp_err_t start(hg::SystemEventBus* bus);

private:
    static void task_entry(void* context);
    void run();

    hg::SystemEventBus* bus_{};
};

}  // namespace homeguard::idf
