#include "homeguard/sha256.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#if defined(ESP_PLATFORM)
#include "esp_log.h"
#include "esp_timer.h"
#include "mbedtls/sha256.h"
#endif

namespace hg {

#if defined(ESP_PLATFORM)
namespace {
constexpr const char* kAccessDiagTag = "homeguard_access";
constexpr char kPinKdfPrefix[] = "HomeGuard-S3|PIN|";
std::uint32_t g_kdf_hash_index = 0U;
std::int64_t g_kdf_started_us = 0;

void trace_pin_kdf(std::span<const std::byte> data) {
    const auto prefix_len = sizeof(kPinKdfPrefix) - 1U;
    const bool initial = data.size() >= prefix_len &&
        std::memcmp(data.data(), kPinKdfPrefix, prefix_len) == 0;

    if (initial) {
        g_kdf_hash_index = 1U;
        g_kdf_started_us = esp_timer_get_time();
        ESP_LOGI(kAccessDiagTag, "PIN KDF SHA begin hash=1 input=%u",
                 static_cast<unsigned>(data.size()));
        return;
    }

    // For the field bootstrap test the user id is "admin" (5 chars), so each
    // iterative KDF input is 64 digest-hex chars + 5 id chars + 32 salt chars.
    // This keeps diagnostics isolated from unrelated SHA traffic.
    if (g_kdf_hash_index != 0U && data.size() == 101U) {
        ++g_kdf_hash_index;
        if ((g_kdf_hash_index % 512U) == 0U || g_kdf_hash_index == 4096U) {
            const auto elapsed_ms = (esp_timer_get_time() - g_kdf_started_us) / 1000;
            ESP_LOGI(kAccessDiagTag, "PIN KDF SHA progress hash=%u elapsed=%lld ms",
                     static_cast<unsigned>(g_kdf_hash_index),
                     static_cast<long long>(elapsed_ms));
        }
        if (g_kdf_hash_index >= 4096U) {
            g_kdf_hash_index = 0U;
        }
    }
}
}  // namespace
#endif

#if !defined(ESP_PLATFORM)
namespace {
constexpr std::array<uint32_t, 64> k{
    0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
    0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
    0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
    0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
    0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
    0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
    0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
    0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
};

constexpr uint32_t rotr(uint32_t value, uint32_t bits) {
    return (value >> bits) | (value << (32U - bits));
}

void compress_block(std::array<uint32_t, 8>& h, const uint8_t* block) {
    std::array<uint32_t, 64> w{};
    for (size_t i = 0; i < 16U; ++i) {
        const size_t p = i * 4U;
        w[i] = (static_cast<uint32_t>(block[p]) << 24U) |
               (static_cast<uint32_t>(block[p + 1U]) << 16U) |
               (static_cast<uint32_t>(block[p + 2U]) << 8U) |
               static_cast<uint32_t>(block[p + 3U]);
    }
    for (size_t i = 16U; i < 64U; ++i) {
        const uint32_t s0 = rotr(w[i - 15U], 7U) ^ rotr(w[i - 15U], 18U) ^ (w[i - 15U] >> 3U);
        const uint32_t s1 = rotr(w[i - 2U], 17U) ^ rotr(w[i - 2U], 19U) ^ (w[i - 2U] >> 10U);
        w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
    }

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
    for (size_t i = 0; i < 64U; ++i) {
        const uint32_t s1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
        const uint32_t ch = (e & f) ^ ((~e) & g);
        const uint32_t temp1 = hh + s1 + ch + k[i] + w[i];
        const uint32_t s0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = s0 + maj;
        hh = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
    }

    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}
}  // namespace
#endif

Sha256Digest sha256(std::span<const std::byte> data) {
#if defined(ESP_PLATFORM)
    trace_pin_kdf(data);
    Sha256Digest digest{};
    const auto* input = reinterpret_cast<const unsigned char*>(data.data());
    if (mbedtls_sha256(input, data.size(), digest.data(), 0) != 0) {
        digest.fill(0U);
    }
    return digest;
#else
    std::array<uint32_t, 8> h{
        0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
        0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U
    };

    std::array<uint8_t, 64> block{};
    size_t offset = 0;
    while (data.size() - offset >= block.size()) {
        for (size_t i = 0; i < block.size(); ++i) {
            block[i] = static_cast<uint8_t>(data[offset + i]);
        }
        compress_block(h, block.data());
        offset += block.size();
    }

    std::array<uint8_t, 128> tail{};
    const size_t remaining = data.size() - offset;
    for (size_t i = 0; i < remaining; ++i) {
        tail[i] = static_cast<uint8_t>(data[offset + i]);
    }
    tail[remaining] = 0x80U;

    const size_t padded_size = remaining < 56U ? 64U : 128U;
    const uint64_t bit_length = static_cast<uint64_t>(data.size()) * 8U;
    for (size_t i = 0; i < 8U; ++i) {
        tail[padded_size - 8U + i] = static_cast<uint8_t>(bit_length >> (56U - 8U * i));
    }

    compress_block(h, tail.data());
    if (padded_size == 128U) compress_block(h, tail.data() + 64U);

    Sha256Digest digest{};
    for (size_t i = 0; i < h.size(); ++i) {
        digest[i * 4U] = static_cast<uint8_t>(h[i] >> 24U);
        digest[i * 4U + 1U] = static_cast<uint8_t>(h[i] >> 16U);
        digest[i * 4U + 2U] = static_cast<uint8_t>(h[i] >> 8U);
        digest[i * 4U + 3U] = static_cast<uint8_t>(h[i]);
    }
    return digest;
#endif
}

Sha256Digest sha256(std::string_view text) {
    return sha256(std::span<const std::byte>(reinterpret_cast<const std::byte*>(text.data()), text.size()));
}

bool constant_time_equal(const Sha256Digest& lhs, const Sha256Digest& rhs) {
    uint8_t difference = 0;
    for (size_t i = 0; i < lhs.size(); ++i) {
        difference |= static_cast<uint8_t>(lhs[i] ^ rhs[i]);
    }
    return difference == 0;
}

std::string sha256_hex(const Sha256Digest& digest) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out(digest.size() * 2U, '0');
    for (size_t i = 0; i < digest.size(); ++i) {
        out[i * 2U] = digits[(digest[i] >> 4U) & 0x0fU];
        out[i * 2U + 1U] = digits[digest[i] & 0x0fU];
    }
    return out;
}
}  // namespace hg
