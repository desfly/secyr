#pragma once

#include "esp_err.h"

#include <cstdint>
#include <string>

namespace homeguard::idf {

struct CloudCommandTrust {
    std::uint32_t version{};
    std::string public_key_pem;
};

class CloudTrustStore {
public:
    esp_err_t load(CloudCommandTrust& trust) const;
    esp_err_t save(const CloudCommandTrust& trust) const;
    esp_err_t clear() const;
};

}  // namespace homeguard::idf
