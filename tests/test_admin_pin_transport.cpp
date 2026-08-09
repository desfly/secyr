#include "test_framework.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/admin_pin_transport.hpp"

#include <array>
#include <string>

void test_admin_pin_transport() {
    homeguard::AccessControl access{};
    std::array<std::uint8_t, 16> salt{};
    salt[0] = 0x5a;
    CHECK(access.set_user("admin", "Administrator", homeguard::AccessRole::Admin, "2468", salt, true));
    const auto* admin = access.find_user("admin");
    CHECK(admin != nullptr);
    if (admin == nullptr) return;

    const auto key = homeguard::admin_pin_transport_key_hex(
        admin->pin_digest, "req-001", "access.users.upsert");
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
