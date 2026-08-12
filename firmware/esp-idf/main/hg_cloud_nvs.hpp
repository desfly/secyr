#pragma once

#include "esp_err.h"

#include <string>

namespace homeguard::idf {

struct CloudConfig {
    bool enabled{};
    std::string broker_uri;
    std::string username;
    std::string password;
};

class CloudNvsStore {
public:
    esp_err_t load(CloudConfig& config) const;
    esp_err_t save(const CloudConfig& config) const;
    esp_err_t clear() const;
};

}  // namespace homeguard::idf
