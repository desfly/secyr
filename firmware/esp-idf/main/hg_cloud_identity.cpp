#include "hg_cloud_identity.hpp"

#include "nvs_config_store.hpp"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace homeguard::idf {
namespace {
std::string hex_encode(const unsigned char* data, std::size_t size)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string out(size * 2U, '\0');
    for (std::size_t i = 0; i < size; ++i) {
        out[i * 2U] = digits[(data[i] >> 4U) & 0x0fU];
        out[i * 2U + 1U] = digits[data[i] & 0x0fU];
    }
    return out;
}
}

esp_err_t make_cloud_identity_proof(
    std::string_view device_id,
    std::string_view challenge,
    CloudIdentityProof& proof)
{
    proof = {};
    if (device_id.empty() || challenge.size() < 16U || challenge.size() > 256U) {
        return ESP_ERR_INVALID_ARG;
    }

    NvsConfigStore store;
    FactoryProvisioningIdentity identity;
    if (!store.load_factory_identity(identity)) return ESP_ERR_NOT_FOUND;

    const std::string transcript =
        "HomeGuard-S3|cloud-enrollment|v1|" + std::string(device_id) + "|" + std::string(challenge);
    std::array<unsigned char, 32> digest{};
    if (mbedtls_sha256(
            reinterpret_cast<const unsigned char*>(transcript.data()),
            transcript.size(), digest.data(), 0) != 0) {
        identity.clear_private_material();
        return ESP_FAIL;
    }

    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    const auto* pem = reinterpret_cast<const unsigned char*>(identity.private_key_pem.c_str());
    const auto pem_size = identity.private_key_pem.size() + 1U;
    int rc = mbedtls_pk_parse_key(&key, pem, pem_size, nullptr, 0, nullptr, nullptr);
    std::array<unsigned char, MBEDTLS_PK_SIGNATURE_MAX_SIZE> signature{};
    std::size_t signature_size = 0;
    if (rc == 0) {
        rc = mbedtls_pk_sign(
            &key, MBEDTLS_MD_SHA256, digest.data(), digest.size(),
            signature.data(), signature.size(), &signature_size, nullptr, nullptr);
    }
    mbedtls_pk_free(&key);
    identity.clear_private_material();
    std::fill(digest.begin(), digest.end(), 0U);

    if (rc != 0 || signature_size == 0U) {
        std::fill(signature.begin(), signature.end(), 0U);
        return ESP_FAIL;
    }

    proof.certificate_pem = identity.certificate_pem;
    proof.signature_der_hex = hex_encode(signature.data(), signature_size);
    std::fill(signature.begin(), signature.end(), 0U);
    return ESP_OK;
}

}  // namespace homeguard::idf
