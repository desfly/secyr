#include "homeguard/admin_pin_transport.hpp"

#include <cstddef>
#include <cstdint>

namespace homeguard {
namespace {

int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
    if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
    return -1;
}

char hex_digit(std::uint8_t nibble)
{
    static constexpr char digits[] = "0123456789abcdef";
    return digits[nibble & 0x0fU];
}

bool decode_key(std::string_view key_hex, hg::Sha256Digest& key)
{
    if (key_hex.size() != key.size() * 2U) return false;
    for (std::size_t i = 0; i < key.size(); ++i) {
        const int high = hex_value(key_hex[i * 2U]);
        const int low = hex_value(key_hex[i * 2U + 1U]);
        if (high < 0 || low < 0) return false;
        key[i] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return true;
}

}  // namespace

std::string admin_pin_transport_key_hex(
    const hg::Sha256Digest& admin_pin_digest,
    std::string_view request_id,
    std::string_view command)
{
    std::string material{"HomeGuard-S3|ADMIN-NEW-PIN|"};
    material += hg::sha256_hex(admin_pin_digest);
    material.push_back('|');
    material.append(request_id);
    material.push_back('|');
    material.append(command);
    return hg::sha256_hex(hg::sha256(material));
}

std::string admin_pin_encrypt_hex(std::string_view pin, std::string_view key_hex)
{
    hg::Sha256Digest key{};
    if (!decode_key(key_hex, key) || pin.empty() || pin.size() > key.size()) return {};

    std::string encrypted;
    encrypted.reserve(pin.size() * 2U);
    for (std::size_t i = 0; i < pin.size(); ++i) {
        const auto byte = static_cast<std::uint8_t>(static_cast<unsigned char>(pin[i])) ^ key[i];
        encrypted.push_back(hex_digit(byte >> 4U));
        encrypted.push_back(hex_digit(byte));
    }
    return encrypted;
}

bool admin_pin_decrypt_hex(
    std::string_view encrypted_hex,
    std::string_view key_hex,
    std::string& pin_out)
{
    pin_out.clear();
    hg::Sha256Digest key{};
    if (!decode_key(key_hex, key) || encrypted_hex.empty() ||
        encrypted_hex.size() % 2U != 0U || encrypted_hex.size() / 2U > key.size()) {
        return false;
    }

    const std::size_t count = encrypted_hex.size() / 2U;
    pin_out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const int high = hex_value(encrypted_hex[i * 2U]);
        const int low = hex_value(encrypted_hex[i * 2U + 1U]);
        if (high < 0 || low < 0) {
            pin_out.clear();
            return false;
        }
        const auto encrypted = static_cast<std::uint8_t>((high << 4) | low);
        const auto plain = static_cast<unsigned char>(encrypted ^ key[i]);
        if (plain < 0x20U || plain > 0x7eU) {
            pin_out.clear();
            return false;
        }
        pin_out.push_back(static_cast<char>(plain));
    }
    return true;
}

}  // namespace homeguard
