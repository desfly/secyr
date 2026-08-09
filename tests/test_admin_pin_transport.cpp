#include "test_framework.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/admin_pin_transport.hpp"

#include <array>
#include <string>

void test_admin_pin_transport() {
    std::array<std::uint8_t, 16> salt{};
    salt[0] = 0x5a;
    const auto digest = homeguard::AccessControl::derive_pin_digest("admin", "2468", salt);
    const auto key = homeguard::admin_pin_transport_key_hex(digest, "req-001", "access.users.upsert");
    CHECK(key.size() == 64U);

    const auto encrypted = homeguard::admin_pin_encrypt_hex("1357", key);
    CHECK(encrypted.size() == 8U);
    CHECK(encrypted != "1357");

    std::string decrypted;
    CHECK(homeguard::admin_pin_decrypt_hex(encrypted, key, decrypted));
    CHECK(decrypted == "1357");

    std::string rejected;
    CHECK(!homeguard::admin_pin_decrypt_hex("zz", key, rejected));
    CHECK(rejected.empty());
}
