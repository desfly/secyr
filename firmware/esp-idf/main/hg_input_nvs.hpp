#pragma once

#include "hg_input_runtime.hpp"
#include "esp_err.h"

namespace homeguard::idf {

class InputNvsStore {
public:
    esp_err_t load(InputPolarityConfig& config) const;
    esp_err_t save(const InputPolarityConfig& config) const;
};

}  // namespace homeguard::idf
