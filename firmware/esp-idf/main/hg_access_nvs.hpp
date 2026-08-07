#pragma once

#include "homeguard/access_control.hpp"
#include "esp_err.h"

namespace homeguard::idf {

class AccessNvsStore {
public:
    esp_err_t load(AccessControl& access) const;
    esp_err_t save(const AccessControl& access) const;
    esp_err_t erase() const;
};

}  // namespace homeguard::idf
