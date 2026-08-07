#include "test_framework.hpp"
#include "homeguard/access_store.hpp"

#include <array>
#include <cstdint>

void test_build0038() {
    homeguard::AccessControl source;
    const std::array<std::uint8_t, 16> admin_salt{1,3,5,7,9,11,13,15,2,4,6,8,10,12,14,16};
    const std::array<std::uint8_t, 16> user_salt{16,14,12,10,8,6,4,2,15,13,11,9,7,5,3,1};

    CHECK(source.set_user("admin", "Administrator", homeguard::AccessRole::Admin, "246810", admin_salt));
    CHECK(source.set_user("resident", "Resident", homeguard::AccessRole::User, "1357", user_salt));

    const auto image = homeguard::AccessStoreCodec::encode(source);
    homeguard::AccessControl restored;
    CHECK(homeguard::AccessStoreCodec::decode(image, restored));
    CHECK(restored.user_count() == 2U);

    const auto* admin = restored.find_user("admin");
    const auto* resident = restored.find_user("resident");
    CHECK(admin != nullptr);
    CHECK(resident != nullptr);
    CHECK(restored.verify_pin(*admin, "246810"));
    CHECK(restored.verify_pin(*resident, "1357"));
    CHECK(!restored.verify_pin(*resident, "0000"));
    CHECK(admin->role == homeguard::AccessRole::Admin);
    CHECK(resident->role == homeguard::AccessRole::User);

    auto damaged = image;
    damaged[20] ^= std::byte{0x01};
    homeguard::AccessControl rejected;
    CHECK(!homeguard::AccessStoreCodec::decode(damaged, rejected));
    CHECK(rejected.user_count() == 0U);

    auto bad_version = image;
    bad_version[4] = std::byte{99};
    CHECK(!homeguard::AccessStoreCodec::decode(bad_version, rejected));
}
