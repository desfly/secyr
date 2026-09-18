#include "hg_cloud_command_verifier.hpp"

#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace homeguard::idf {
namespace {

int hex_nibble(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool decode_hex(std::string_view input, std::vector<unsigned char>& output)
{
    output.clear();
    if (input.empty() || (input.size() % 2U) != 0U ||
        input.size() > MBEDTLS_PK_SIGNATURE_MAX_SIZE * 2U) return false;
    output.reserve(input.size() / 2U);
    for (std::size_t i = 0; i < input.size(); i += 2U) {
        const int high = hex_nibble(input[i]);
        const int low = hex_nibble(input[i + 1U]);
        if (high < 0 || low < 0) {
            std::fill(output.begin(), output.end(), 0U);
            output.clear();
            return false;
        }
        output.push_back(static_cast<unsigned char>((high << 4) | low));
    }
    return true;
}

}  // namespace

esp_err_t CloudCommandVerifier::verify(
    std::string_view public_key_pem,
    std::string_view canonical_envelope,
    std::string_view signature_der_hex) const
{
    if (public_key_pem.empty() || canonical_envelope.empty() || signature_der_hex.empty()) {
        return ESP_ERR_INVALID_ARG;
    }

    std::vector<unsigned char> signature;
    if (!decode_hex(signature_der_hex, signature)) return ESP_ERR_INVALID_ARG;

    std::array<unsigned char, 32> digest{};
    if (mbedtls_sha256(
            reinterpret_cast<const unsigned char*>(canonical_envelope.data()),
            canonical_envelope.size(), digest.data(), 0) != 0) {
        std::fill(signature.begin(), signature.end(), 0U);
        return ESP_FAIL;
    }

    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    const auto* pem = reinterpret_cast<const unsigned char*>(public_key_pem.data());
    const int parse_rc = mbedtls_pk_parse_public_key(&key, pem, public_key_pem.size() + 1U);
    int verify_rc = parse_rc;
    if (parse_rc == 0) {
        verify_rc = mbedtls_pk_verify(
            &key, MBEDTLS_MD_SHA256, digest.data(), digest.size(),
            signature.data(), signature.size());
    }
    mbedtls_pk_free(&key);
    std::fill(digest.begin(), digest.end(), 0U);
    std::fill(signature.begin(), signature.end(), 0U);

    return verify_rc == 0 ? ESP_OK : ESP_ERR_INVALID_CRC;
}

}  // namespace homeguard::idf
