#pragma once

#include "homeguard/config_exchange.hpp"
#include "esp_err.h"

namespace homeguard::idf {

class ConfigNvsStore {
public:
    esp_err_t load(HomeGuardConfigDocument& document) const;
    esp_err_t save(const HomeGuardConfigDocument& document) const;
};

}  // namespace homeguard::idf
