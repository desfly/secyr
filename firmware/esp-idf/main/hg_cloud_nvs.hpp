#pragma once

#include "esp_err.h"

#include <array>
#include <cstdint>

namespace homeguard::idf {

struct CloudConfigRecord {
    std::array<char, 160> broker_uri{};
    std::array<char, 64> username{};
    std::array<char, 96> password{};
    bool enabled{};
};

class CloudNvsStore {
public:
    esp_err_t load(CloudConfigRecord& config) const;
    esp_err_t save(const CloudConfigRecord& config) const;
    esp_err_t clear() const;
};

}  // namespace homeguard::idf
