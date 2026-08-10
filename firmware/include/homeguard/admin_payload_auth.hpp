#pragma once

#include "homeguard/sha256.hpp"

#include <string>
#include <string_view>

namespace homeguard {

[[nodiscard]] std::string admin_payload_proof_hex(
    const hg::Sha256Digest& admin_pin_digest,
    std::string_view request_id,
    std::string_view command,
    std::string_view canonical_payload);

[[nodiscard]] bool verify_admin_payload_proof(
    const hg::Sha256Digest& admin_pin_digest,
    std::string_view request_id,
    std::string_view command,
    std::string_view canonical_payload,
    std::string_view proof_hex);

}  // namespace homeguard
