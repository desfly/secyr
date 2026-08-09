#include "test_framework.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/admin_payload_auth.hpp"

#include <array>

void test_admin_payload_auth() {
    homeguard::AccessControl access{};
    std::array<std::uint8_t, 16> salt{};
    CHECK(access.set_user("admin", "Admin", homeguard::AccessRole::Admin, "2468", salt, true));
    const auto* admin = access.find_user("admin");
    CHECK(admin != nullptr);
    if (admin == nullptr) return;

    const auto proof = homeguard::admin_payload_proof_hex(
        admin->pin_digest, "req-1", "access.users.delete", "user1");
    CHECK(proof.size() == 64U);
    CHECK(homeguard::verify_admin_payload_proof(
        admin->pin_digest, "req-1", "access.users.delete", "user1", proof));
    CHECK(!homeguard::verify_admin_payload_proof(
        admin->pin_digest, "req-1", "access.users.delete", "user2", proof));
    CHECK(!homeguard::verify_admin_payload_proof(
        admin->pin_digest, "req-2", "access.users.delete", "user1", proof));
}
