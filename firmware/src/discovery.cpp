#include "homeguard/discovery.hpp"

namespace hg {
namespace {
std::string escaped(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) >= 0x20U) out += c;
                break;
        }
    }
    return out;
}
}

bool is_discovery_request(std::string_view payload) {
    while (!payload.empty() && (payload.back() == '\r' || payload.back() == '\n' || payload.back() == ' ')) payload.remove_suffix(1);
    while (!payload.empty() && payload.front() == ' ') payload.remove_prefix(1);
    return payload == discovery_request_v1;
}

std::string make_discovery_response(const DiscoveryRecord& r) {
    std::string out = "{\"protocol\":\"homeguard-discovery-v1\",\"device_id\":\"";
    out += escaped(r.device_id);
    out += "\",\"hostname\":\"";
    out += escaped(r.hostname);
    out += "\",\"port\":" + std::to_string(r.port);
    out += ",\"secure\":";
    out += r.secure ? "true" : "false";
    out += ",\"transport\":\"";
    out += to_string(r.transport);
    out += "\",\"api_version\":" + std::to_string(r.api_version);
    out += ",\"pairing_required\":";
    out += r.pairing_required ? "true" : "false";
    out += "}";
    return out;
}
}
