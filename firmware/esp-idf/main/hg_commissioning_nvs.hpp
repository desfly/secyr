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

    // Factory reset must preserve the immutable/board-specific hardware
    // verification record while clearing only user-owned commissioning state.
    esp_err_t erase_commissioning_state() const;

    // Service-only destructive erase. Do not use for normal Factory Reset.
    esp_err_t erase_all() const;
};

}  // namespace homeguard::idf
