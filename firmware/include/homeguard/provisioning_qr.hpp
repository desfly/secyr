#pragma once
#include <string>
#include <string_view>

namespace hg {
struct ProvisioningQrData {
    std::string device_id;
    std::string setup_ssid;
    std::string setup_url;
    std::string setup_password;
    std::string certificate_sha256;
    std::string pairing_code;
};
[[nodiscard]] std::string make_provisioning_uri(const ProvisioningQrData& data);
[[nodiscard]] std::string url_encode(std::string_view value);
}
