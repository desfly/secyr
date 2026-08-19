#include "../firmware/esp-idf/main/hg_access_runtime.hpp"
#include "homeguard/access_control.hpp"
#include "test_framework.hpp"

#include <array>

void test_access_runtime() {
    homeguard::AccessControl access;
    homeguard::idf::access_runtime::set_bootstrap_allowed(true);

    for (int i = 0; i < 1000; ++i) {
        CHECK(homeguard::idf::access_runtime::setup_required(access));
        CHECK(homeguard::idf::access_runtime::bootstrap_allowed());
    }

    std::array<std::uint8_t, 16> salt{};
    CHECK(access.set_user("admin", "Admin", homeguard::AccessRole::Admin, "1234", salt, true));
    homeguard::idf::access_runtime::lock_bootstrap();

    CHECK(!homeguard::idf::access_runtime::setup_required(access));
    CHECK(!homeguard::idf::access_runtime::bootstrap_allowed());

    for (int i = 0; i < 1000; ++i) {
        CHECK(!homeguard::idf::access_runtime::setup_required(access));
    }
}
