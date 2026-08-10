#pragma once

#include "homeguard/sha256.hpp"

#include <string>
#include <string_view>

namespace homeguard {

[[nodiscard]] std::string admin_pin_transport_key_hex(
    const hg::Sha256Digest& admin_pin_digest,
    std::string_view request_id,
    std::string_view command);

[[nodiscard]] std::string admin_pin_encrypt_hex(
    std::string_view pin,
    std::string_view key_hex);

[[nodiscard]] bool admin_pin_decrypt_hex(
    std::string_view encrypted_hex,
    std::string_view key_hex,
    std::string& pin_out);

}  // namespace homeguard
