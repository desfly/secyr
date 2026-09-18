#pragma once

#include "esp_err.h"

#include <cstdint>

namespace homeguard::idf {

class CloudTrustedTime {
public:
    esp_err_t start();
    bool ready() const;
    std::uint64_t now_ms() const;
};

}  // namespace homeguard::idf
