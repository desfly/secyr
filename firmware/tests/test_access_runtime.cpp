#include "hg_access_runtime.hpp"
#include "homeguard/access_control.hpp"

#include <array>
#include <cassert>

int main() {
    using namespace homeguard;
    using namespace homeguard::idf;

    AccessControl access;
    access_runtime::set_bootstrap_allowed(true);

    // Repeated visits/reloads do not consume factory setup.
    for (int i = 0; i < 1000; ++i) {
        assert(access_runtime::setup_required(access));
        assert(access_runtime::bootstrap_allowed());
    }

    // Failed or abandoned setup leaves the controller in setup_required.
    assert(access.user_count() == 0U);
    assert(access_runtime::setup_required(access));

    // Only a successfully created first Admin closes passwordless setup.
    std::array<std::uint8_t, 16> salt{};
    assert(access.set_user("admin", "Admin", AccessRole::Admin, "1234", salt, true));
    access_runtime::lock_bootstrap();
    assert(!access_runtime::setup_required(access));
    assert(!access_runtime::bootstrap_allowed());

    // Ordinary later visits cannot reopen bootstrap.
    for (int i = 0; i < 1000; ++i) {
        assert(!access_runtime::setup_required(access));
    }

    return 0;
}
