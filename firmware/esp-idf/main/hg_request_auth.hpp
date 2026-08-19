#pragma once

#include "homeguard/access_control.hpp"
#include "esp_http_server.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace homeguard::idf::request_auth {

inline bool read_authorization(httpd_req_t* request, std::string& actor, std::string& credential) {
    actor.clear();
    credential.clear();
    if (request == nullptr) return false;

    const auto length = httpd_req_get_hdr_value_len(request, "Authorization");
    if (length == 0U || length > 64U) return false;

    std::array<char, 66> buffer{};
    if (httpd_req_get_hdr_value_str(request, "Authorization", buffer.data(), buffer.size()) != ESP_OK) return false;

    constexpr std::string_view prefix{"HomeGuard "};
    const std::string_view value{buffer.data(), length};
    if (!value.starts_with(prefix)) return false;

    const auto payload = value.substr(prefix.size());
    const auto separator = payload.find(':');
    if (separator == std::string_view::npos || separator == 0U || separator + 1U >= payload.size()) return false;

    const auto actor_view = payload.substr(0U, separator);
    const auto credential_view = payload.substr(separator + 1U);
    if (actor_view.size() > 23U || credential_view.size() < 4U || credential_view.size() > 12U) return false;
    for (const char ch : credential_view) if (ch < '0' || ch > '9') return false;

    actor.assign(actor_view);
    credential.assign(credential_view);
    return true;
}

inline homeguard::AuditDecision authenticate(httpd_req_t* request, homeguard::AccessControl& access) {
    std::string actor;
    std::string credential;
    if (!read_authorization(request, actor, credential)) return homeguard::AuditDecision::DeniedCredential;
    const auto decision = access.authenticate(actor, credential);
    std::fill(credential.begin(), credential.end(), '\0');
    credential.clear();
    return decision;
}

inline bool authenticated(httpd_req_t* request, homeguard::AccessControl& access) {
    return authenticate(request, access) == homeguard::AuditDecision::Allowed;
}

inline esp_err_t send_login_required(httpd_req_t* request) {
    httpd_resp_set_status(request, "401 Unauthorized");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"login_required\"}", HTTPD_RESP_USE_STRLEN);
}

}  // namespace homeguard::idf::request_auth
