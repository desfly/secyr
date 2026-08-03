#include "test_framework.hpp"
#include "homeguard/provisioning.hpp"
#include "homeguard/provisioning_qr.hpp"
#include "homeguard/sha256.hpp"
#include <string>

void test_provisioning() {
    CHECK(hg::sha256_hex(hg::sha256("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(hg::sha256_hex(hg::sha256("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(hg::constant_time_equal(hg::sha256("same"), hg::sha256("same")));
    CHECK(!hg::constant_time_equal(hg::sha256("same"), hg::sha256("other")));
    CHECK(hg::valid_pairing_code("01234567"));
    CHECK(!hg::valid_pairing_code("1234567"));
    CHECK(!hg::valid_pairing_code("1234567x"));
    CHECK(hg::format_pairing_code(42) == "00000042");
    CHECK(hg::valid_sha256_hex(std::string(64, 'a')));
    CHECK(!hg::valid_sha256_hex(std::string(63, 'a')));

    const std::string fingerprint(64, 'a');
    hg::ProvisioningSession session({600000, 900000, 3, false});
    CHECK(session.begin("12345678", fingerprint, 1000) == hg::ProvisioningCode::Accepted);
    CHECK(session.status(1000).state == hg::ProvisioningState::SetupAp);
    CHECK(session.authorize("00000000", fingerprint, 2000) == hg::ProvisioningCode::InvalidProof);
    CHECK(session.status(2000).failed_attempts == 1);
    CHECK(session.authorize("12345678", std::string(64, 'b'), 3000) == hg::ProvisioningCode::InvalidProof);
    CHECK(session.authorize("12345678", fingerprint, 4000) == hg::ProvisioningCode::Accepted);
    CHECK(session.status(4000).authorized);

    hg::ProvisioningPayload invalid{"Home", "short", "", "", std::string(32, 'L'), "House"};
    CHECK(session.submit(invalid, 5000) == hg::ProvisioningCode::InvalidPayload);
    hg::ProvisioningPayload payload{"Home WiFi", "correct horse battery", "mqtts://cloud.homeguard.example:8883", std::string(40, 'C'), std::string(48, 'L'), "Main house"};
    CHECK(payload.valid({600000, 900000, 3, false}));
    CHECK(session.submit(payload, 6000) == hg::ProvisioningCode::Accepted);
    CHECK(session.status(6000).has_pending_payload);
    CHECK(session.pending()->wifi_ssid == "Home WiFi");
    CHECK(session.commit(true, 7000) == hg::ProvisioningCode::Accepted);
    CHECK(session.status(7000).state == hg::ProvisioningState::Provisioned);

    hg::ProvisioningShutdownGate shutdown_gate;
    CHECK(!shutdown_gate.armed());
    shutdown_gate.arm(7000, 1500);
    CHECK(shutdown_gate.armed());
    CHECK(!shutdown_gate.due(8499));
    CHECK(shutdown_gate.due(8500));
    shutdown_gate.clear();
    CHECK(!shutdown_gate.due(9000));
    CHECK(!session.factory_reset(false));
    CHECK(session.factory_reset(true));
    CHECK(session.status(7000).state == hg::ProvisioningState::Factory);

    hg::ProvisioningSession locked({100, 200, 2, false});
    CHECK(locked.begin("87654321", fingerprint, 0) == hg::ProvisioningCode::Accepted);
    CHECK(locked.authorize("00000000", fingerprint, 10) == hg::ProvisioningCode::InvalidProof);
    CHECK(locked.authorize("00000001", fingerprint, 20) == hg::ProvisioningCode::LockedOut);
    CHECK(locked.authorize("87654321", fingerprint, 30) == hg::ProvisioningCode::LockedOut);


    hg::ProvisioningSession timeout({100, 200, 5, false});
    CHECK(timeout.begin("11223344", fingerprint, 1000) == hg::ProvisioningCode::Accepted);
    CHECK(!timeout.expire_if_needed(1200));
    CHECK(timeout.expire_if_needed(1201));
    CHECK(timeout.status(1201).state == hg::ProvisioningState::Locked);

    hg::ProvisioningSession expired({100, 200, 5, false});
    CHECK(expired.begin("87654321", fingerprint, 0) == hg::ProvisioningCode::Accepted);
    CHECK(expired.authorize("87654321", fingerprint, 101) == hg::ProvisioningCode::Expired);

    hg::ProvisioningQrData qr{"HG-S3-7A31BC", "HomeGuard-S3-7A31BC-Setup", "https://192.168.4.1:8443", "AP-PASSWORD-1234", fingerprint, "12345678"};
    const auto uri = hg::make_provisioning_uri(qr);
    CHECK(uri.starts_with("homeguard://provision?v=1"));
    CHECK(uri.find("id=HG-S3-7A31BC") != std::string::npos);
    CHECK(uri.find("url=https%3A%2F%2F192.168.4.1%3A8443") != std::string::npos);
    CHECK(uri.find("pw=AP-PASSWORD-1234") != std::string::npos);
    CHECK(uri.find("code=12345678") != std::string::npos);
    CHECK(uri.find("cloud_token") == std::string::npos);
}
