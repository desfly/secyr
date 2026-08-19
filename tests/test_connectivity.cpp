#include "test_framework.hpp"
#include "homeguard/discovery.hpp"
#include <string>

void test_connectivity() {
    CHECK(hg::is_discovery_request("HG_DISCOVER_V1"));
    CHECK(hg::is_discovery_request(" HG_DISCOVER_V1\r\n"));
    CHECK(!hg::is_discovery_request("HG_DISCOVER_V2"));

    hg::DiscoveryRecord record{"HG-S3-7A31BC", "homeguard-7a31bc", 443, hg::Transport::Ethernet, true, false, 1};
    const std::string json = hg::make_discovery_response(record);
    CHECK(json.find("\"device_id\":\"HG-S3-7A31BC\"") != std::string::npos);
    CHECK(json.find("\"port\":443") != std::string::npos);
    CHECK(json.find("\"secure\":true") != std::string::npos);
    CHECK(json.find("\"transport\":\"ethernet\"") != std::string::npos);
}
