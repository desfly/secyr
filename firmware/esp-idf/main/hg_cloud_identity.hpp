#pragma once

#include "esp_err.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace homeguard::idf {

struct CloudIdentityProof {
    std::string certificate_pem;
    std::string signature_der_hex;
};

esp_err_t make_cloud_identity_proof(
    std::string_view device_id,
    std::string_view challenge,
    CloudIdentityProof& proof);

}  // namespace homeguard::idf
