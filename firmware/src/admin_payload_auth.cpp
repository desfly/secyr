#include "homeguard/admin_payload_auth.hpp"
#include "homeguard/admin_payload_canonical.hpp"

#include <array>
#include <cstddef>
#include <string>

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

bool split_upsert_legacy(std::string_view legacy, std::array<std::string_view, 5>& fields)
{
    std::size_t start = 0U;
    for (std::size_t i = 0; i < fields.size() - 1U; ++i) {
        const auto pos = legacy.find('\n', start);
        if (pos == std::string_view::npos) return false;
        fields[i] = legacy.substr(start, pos - start);
        start = pos + 1U;
    }
    fields.back() = legacy.substr(start);
    return true;
}

std::string canonicalize_for_command(std::string_view command, std::string_view payload)
{
    if (command == "access.users.upsert") {
        std::array<std::string_view, 5> fields{};
        if (!split_upsert_legacy(payload, fields)) return {};
        return canonical_admin_upsert_payload(fields[0], fields[1], fields[2], fields[3], fields[4]);
    }
    if (command == "access.users.enable" ||
        command == "access.users.disable" ||
        command == "access.users.delete") {
        return canonical_admin_target_payload(payload);
    }
    return std::string{payload};
}

}  // namespace

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
    const auto normalized = canonicalize_for_command(command, canonical_payload);
    if (normalized.empty() && !canonical_payload.empty()) return false;
    return constant_time_text_equal(
        admin_payload_proof_hex(admin_pin_digest, request_id, command, normalized),
        proof_hex);
}

}  // namespace homeguard
