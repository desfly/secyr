#pragma once

#include "homeguard/commissioning_state.hpp"
#include "homeguard/hardware_verification.hpp"

#include "esp_err.h"

namespace homeguard::idf {

class CommissioningNvsStore {
public:
    esp_err_t load_hardware(hg::HardwareVerificationRecord& record) const;
    esp_err_t save_hardware(const hg::HardwareVerificationRecord& record) const;
    esp_err_t load_commissioning(hg::CommissioningPersistentState& state) const;
    esp_err_t save_commissioning(const hg::CommissioningPersistentState& state) const;
    esp_err_t erase_all() const;
};

}  // namespace homeguard::idf
