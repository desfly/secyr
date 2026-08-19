#include "test_framework.hpp"
#include "homeguard/device_identity.hpp"
#include "homeguard/discovery.hpp"
#include <array>
#include <string>

void test_connectivity() {
    hg::DeviceIdentity id({0x24,0x6F,0x28,0x7A,0x31,0xBC});
    CHECK(id.device_id() == "HG-S3-7A31BC");
    CHECK(id.hostname() == "homeguard-s3-7a31bc");
    CHECK(id.service_instance() == "HomeGuard-S3 7A31BC");
    CHECK(hg::is_discovery_request("HG_DISCOVER_V1"));
    CHECK(hg::is_discovery_request(" HG_DISCOVER_V1\r\n"));
    CHECK(!hg::is_discovery_request("HG_DISCOVER_V2"));
    hg::DiscoveryRecord record{id.device_id(),id.hostname(),443,hg::Transport::Ethernet,true,false,1};
    const std::string json=hg::make_discovery_response(record);
    CHECK(json.find("\"device_id\":\"HG-S3-7A31BC\"") != std::string::npos);
    CHECK(json.find("\"port\":443") != std::string::npos);
    CHECK(json.find("\"secure\":true") != std::string::npos);
    CHECK(json.find("\"transport\":\"ethernet\"") != std::string::npos);
}
