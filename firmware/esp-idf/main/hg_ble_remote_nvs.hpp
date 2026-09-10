#pragma once

#include "homeguard/ble_remote.hpp"
#include "esp_err.h"

namespace homeguard::idf {

class BleRemoteNvsStore {
public:
    esp_err_t load(hg::BleRemoteRegistry& registry) const;
    esp_err_t save(const hg::BleRemoteRegistry& registry) const;
    esp_err_t erase() const;
};

}  // namespace homeguard::idf
