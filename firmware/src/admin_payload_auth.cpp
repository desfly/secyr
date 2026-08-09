#include "homeguard/admin_payload_auth.hpp"

namespace homeguard {
namespace {

bool constant_time_text_equal(std::string_view left, std::string_view right)
{
    if (left.size() != right.size()) return false;
    unsigned char diff = 0U;
    for (std::size_t i = 0; i < left.size(); ++i) {
        diff |= static_cast<unsigned char>(left[i]) ^ static_cast<unsigned char>(right[i]);
    }
    return diff == 0U;
}

}

std::string admin_payload_proof_hex(
    const hg::Sha256Digest& admin_pin_digest,
    std::string_view request_id,
    std::string_view command,
    std::string_view canonical_payload)
{
    std::string material{"HomeGuard-S3|ADMIN-PAYLOAD|"};
    material += hg::sha256_hex(admin_pin_digest);
    material.push_back('|');
    material.append(request_id);
    material.push_back('|');
    material.append(command);
    material.push_back('|');
    material.append(canonical_payload);
    return hg::sha256_hex(hg::sha256(material));
}

bool verify_admin_payload_proof(
    const hg::Sha256Digest& admin_pin_digest,
    std::string_view request_id,
    std::string_view command,
    std::string_view canonical_payload,
    std::string_view proof_hex)
{
    if (proof_hex.size() != 64U) return false;
    return constant_time_text_equal(
        admin_payload_proof_hex(admin_pin_digest, request_id, command, canonical_payload),
        proof_hex);
}

}  // namespace homeguard
