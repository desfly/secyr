#include "test_framework.hpp"
#include "homeguard/admin_payload_canonical.hpp"

void test_admin_payload_canonical() {
    const auto left = homeguard::canonical_admin_upsert_payload(
        "a", "bc", "user", "true", "00112233");
    const auto right = homeguard::canonical_admin_upsert_payload(
        "ab", "c", "user", "true", "00112233");
    CHECK(left != right);

    const auto target_a = homeguard::canonical_admin_target_payload("a");
    const auto target_ab = homeguard::canonical_admin_target_payload("ab");
    CHECK(target_a != target_ab);

    CHECK(left == "1:a|2:bc|4:user|4:true|8:00112233|");
}
