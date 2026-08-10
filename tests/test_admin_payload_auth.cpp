#include "test_framework.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/admin_payload_auth.hpp"
#include "homeguard/admin_payload_canonical.hpp"

#include <array>

void test_admin_payload_auth() {
    homeguard::AccessControl access{};
    std::array<std::uint8_t, 16> salt{};
    CHECK(access.set_user("admin", "Admin", homeguard::AccessRole::Admin, "2468", salt, true));
    const auto* admin = access.find_user("admin");
    CHECK(admin != nullptr);
    if (admin == nullptr) return;

    const auto canonical = homeguard::canonical_admin_target_payload("user1");
    const auto proof = homeguard::admin_payload_proof_hex(
        admin->pin_digest, "req-1", "access.users.delete", canonical);
    CHECK(proof.size() == 64U);

    // Runtime receives raw target_id from app_main and canonicalizes it internally.
    CHECK(homeguard::verify_admin_payload_proof(
        admin->pin_digest, "req-1", "access.users.delete", "user1", proof));
    CHECK(!homeguard::verify_admin_payload_proof(
        admin->pin_digest, "req-1", "access.users.delete", "user2", proof));
    CHECK(!homeguard::verify_admin_payload_proof(
        admin->pin_digest, "req-2", "access.users.delete", "user1", proof));

    // A proof generated from the legacy raw payload must no longer validate.
    const auto legacy_proof = homeguard::admin_payload_proof_hex(
        admin->pin_digest, "req-1", "access.users.delete", "user1");
    CHECK(!homeguard::verify_admin_payload_proof(
        admin->pin_digest, "req-1", "access.users.delete", "user1", legacy_proof));
}
