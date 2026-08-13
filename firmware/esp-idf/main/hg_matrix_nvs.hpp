#pragma once

#include "homeguard/user_output_access.hpp"
#include "homeguard/user_zone_access.hpp"
#include "esp_err.h"

namespace homeguard::idf {

class MatrixNvsStore {
public:
    esp_err_t load(UserZoneAccess& zones, UserOutputAccess& outputs) const;
    esp_err_t save(const UserZoneAccess& zones, const UserOutputAccess& outputs) const;
};

}  // namespace homeguard::idf
