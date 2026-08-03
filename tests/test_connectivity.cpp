#include "test_framework.hpp"
#include "homeguard/cloud_link.hpp"
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

    hg::CloudLink cloud;
    CHECK(cloud.state()==hg::CloudLinkState::Disabled);
    cloud.configure({true,"mqtts://cloud.homeguard.invalid","HG-S3-7A31BC",8883,1000,8000});
    CHECK(cloud.state()==hg::CloudLinkState::WaitingForNetwork);
    CHECK(cloud.tick(false,0)==hg::CloudAction::None);
    CHECK(cloud.tick(true,1)==hg::CloudAction::Connect);
    cloud.on_connecting(1);
    CHECK(cloud.state()==hg::CloudLinkState::Connecting);
    cloud.on_connected(10);
    CHECK(cloud.can_publish());
    CHECK(cloud.current_backoff_ms()==1000);
    CHECK(cloud.tick(false,11)==hg::CloudAction::Disconnect);
    CHECK(cloud.state()==hg::CloudLinkState::WaitingForNetwork);
    CHECK(cloud.tick(true,12)==hg::CloudAction::Connect);
    cloud.on_connecting(12);
    cloud.on_disconnected(20);
    CHECK(cloud.state()==hg::CloudLinkState::Backoff);
    CHECK(cloud.next_retry_at()==1020);
    CHECK(cloud.current_backoff_ms()==2000);
    CHECK(cloud.tick(true,1019)==hg::CloudAction::None);
    CHECK(cloud.tick(true,1020)==hg::CloudAction::Connect);
    cloud.on_connecting(1020);
    cloud.on_disconnected(1030,true);
    CHECK(cloud.state()==hg::CloudLinkState::Fault);

    hg::CloudLink invalid;
    invalid.configure({true,"","HG-S3-7A31BC",8883,1000,8000});
    CHECK(invalid.state()==hg::CloudLinkState::Fault);
}
