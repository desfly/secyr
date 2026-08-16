#pragma once

#include "esp_err.h"

namespace homeguard::idf {

struct FactoryResetReport {
    esp_err_t access{ESP_OK};
    esp_err_t wifi{ESP_OK};
    esp_err_t cloud{ESP_OK};
    esp_err_t commissioning{ESP_OK};

    [[nodiscard]] bool ok() const {
        return access == ESP_OK && wifi == ESP_OK && cloud == ESP_OK && commissioning == ESP_OK;
    }
};

// Erases all user-owned persistent configuration while preserving firmware and
// immutable hardware verification/identity records.
class FactoryResetManager {
public:
    [[nodiscard]] FactoryResetReport erase_mutable_state() const;
};

}  // namespace homeguard::idf
