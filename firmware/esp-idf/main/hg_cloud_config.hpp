#pragma once

#include "esp_err.h"
#include <array>

namespace homeguard::idf {

struct CloudConfig {
    std::array<char, 160> broker_uri{};
    std::array<char, 65> username{};
    std::array<char, 129> password{};

    [[nodiscard]] bool valid() const noexcept {
        return broker_uri[0] != '\0';
    }
};

class CloudConfigStore {
public:
    esp_err_t load(CloudConfig& config) const;
    esp_err_t save(const CloudConfig& config) const;
    esp_err_t erase() const;
};

}  // namespace homeguard::idf
