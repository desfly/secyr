#include "homeguard/access_store.hpp"
#include "homeguard/crc32.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace homeguard {
namespace {
constexpr std::array<std::byte, 4> magic{
    std::byte{'H'}, std::byte{'G'}, std::byte{'A'}, std::byte{'1'}
};
constexpr std::uint8_t format_version = 1;

void put_u32(std::span<std::byte> out, std::size_t offset, std::uint32_t value) {
    out[offset + 0] = static_cast<std::byte>(value & 0xffU);
    out[offset + 1] = static_cast<std::byte>((value >> 8U) & 0xffU);
    out[offset + 2] = static_cast<std::byte>((value >> 16U) & 0xffU);
    out[offset + 3] = static_cast<std::byte>((value >> 24U) & 0xffU);
}

std::uint32_t get_u32(std::span<const std::byte> in, std::size_t offset) {
    return std::to_integer<std::uint32_t>(in[offset + 0]) |
           (std::to_integer<std::uint32_t>(in[offset + 1]) << 8U) |
           (std::to_integer<std::uint32_t>(in[offset + 2]) << 16U) |
           (std::to_integer<std::uint32_t>(in[offset + 3]) << 24U);
}

void put_bytes(std::span<std::byte> out, std::size_t& cursor, const auto& source) {
    for (const auto value : source) out[cursor++] = static_cast<std::byte>(value);
}

template <std::size_t N>
void get_chars(std::span<const std::byte> in, std::size_t& cursor, std::array<char, N>& out) {
    for (auto& value : out) value = static_cast<char>(std::to_integer<unsigned char>(in[cursor++]));
}

template <std::size_t N>
void get_u8(std::span<const std::byte> in, std::size_t& cursor, std::array<std::uint8_t, N>& out) {
    for (auto& value : out) value = std::to_integer<std::uint8_t>(in[cursor++]);
}

void read_record(std::span<const std::byte> image, std::size_t& cursor, AccessUser& record) {
    get_chars(image, cursor, record.id);
    get_chars(image, cursor, record.name);
    record.role = static_cast<AccessRole>(std::to_integer<std::uint8_t>(image[cursor++]));
    record.enabled = std::to_integer<std::uint8_t>(image[cursor++]) != 0U;
    get_u8(image, cursor, record.salt);
    get_u8(image, cursor, record.pin_digest);
}

bool valid_record_for_import(const AccessUser& user) {
    const bool id_terminated = std::find(user.id.begin(), user.id.end(), '\0') != user.id.end();
    const bool name_terminated = std::find(user.name.begin(), user.name.end(), '\0') != user.name.end();
    return id_terminated && name_terminated && user.id[0] != '\0' &&
           static_cast<std::uint8_t>(user.role) <= static_cast<std::uint8_t>(AccessRole::Admin);
}
}

AccessStoreCodec::Image AccessStoreCodec::encode(const AccessControl& access) {
    Image image{};
    std::copy(magic.begin(), magic.end(), image.begin());
    image[4] = static_cast<std::byte>(format_version);
    image[5] = static_cast<std::byte>(access.user_count());
    image[6] = std::byte{0};
    image[7] = std::byte{0};

    std::size_t cursor = header_size;
    for (std::size_t i = 0; i < AccessControl::user_capacity; ++i) {
        AccessUser empty{};
        const AccessUser* user = access.user_at(i);
        const AccessUser& record = user ? *user : empty;
        put_bytes(image, cursor, record.id);
        put_bytes(image, cursor, record.name);
        image[cursor++] = static_cast<std::byte>(record.role);
        image[cursor++] = record.enabled ? std::byte{1} : std::byte{0};
        put_bytes(image, cursor, record.salt);
        put_bytes(image, cursor, record.pin_digest);
    }

    const auto checksum = hg::crc32(std::span<const std::byte>{image.data(), image.size() - crc_size});
    put_u32(image, image.size() - crc_size, checksum);
    return image;
}

bool AccessStoreCodec::decode(std::span<const std::byte> image, AccessControl& access) {
    if (image.size() != image_size) return false;
    if (!std::equal(magic.begin(), magic.end(), image.begin())) return false;
    if (std::to_integer<std::uint8_t>(image[4]) != format_version) return false;

    const auto count = std::to_integer<std::uint8_t>(image[5]);
    if (count > AccessControl::user_capacity) return false;

    const auto expected = get_u32(image, image.size() - crc_size);
    const auto actual = hg::crc32(image.first(image.size() - crc_size));
    if (expected != actual) return false;

    // First pass validates every persisted user before the destination is
    // mutated. Keep only one AccessUser on the task stack. A full temporary
    // AccessControl also carries the 64-entry audit ring and previously
    // overflowed ESP-IDF's main task while restoring NVS at boot.
    std::size_t cursor = header_size;
    for (std::size_t i = 0; i < AccessControl::user_capacity; ++i) {
        AccessUser record{};
        read_record(image, cursor, record);
        if (i < count && !valid_record_for_import(record)) return false;
    }

    // Second pass performs the import after validation. count is already
    // bounded by user_capacity, so a validated image cannot exhaust capacity.
    access.clear_users();
    cursor = header_size;
    for (std::size_t i = 0; i < count; ++i) {
        AccessUser record{};
        read_record(image, cursor, record);
        if (!access.import_user(record)) {
            access.clear_users();
            return false;
        }
    }
    return true;
}

}  // namespace homeguard
