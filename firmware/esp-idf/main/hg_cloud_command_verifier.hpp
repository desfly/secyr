#pragma once

#include "esp_err.h"

#include <string_view>

namespace homeguard::idf {

class CloudCommandVerifier {
public:
    esp_err_t verify(
        std::string_view public_key_pem,
        std::string_view canonical_envelope,
        std::string_view signature_der_hex) const;
};

}  // namespace homeguard::idf
