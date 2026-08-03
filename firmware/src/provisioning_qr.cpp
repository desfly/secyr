#include "homeguard/provisioning_qr.hpp"
#include <array>
#include <cctype>

namespace hg {
std::string url_encode(std::string_view value) {
    constexpr std::array<char, 16> hex{'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
    std::string out;
    out.reserve(value.size() * 3U);
    for (const unsigned char c : value) {
        if (std::isalnum(c) != 0 || c == '-' || c == '_' || c == '.' || c == '~') out.push_back(static_cast<char>(c));
        else { out.push_back('%'); out.push_back(hex[c >> 4U]); out.push_back(hex[c & 0x0fU]); }
    }
    return out;
}

std::string make_provisioning_uri(const ProvisioningQrData& data) {
    return "homeguard://provision?v=1&id=" + url_encode(data.device_id) +
        "&ssid=" + url_encode(data.setup_ssid) +
        "&url=" + url_encode(data.setup_url) +
        "&pw=" + url_encode(data.setup_password) +
        "&fp=" + url_encode(data.certificate_sha256) +
        "&code=" + url_encode(data.pairing_code);
}
}
