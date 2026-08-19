#pragma once

#include "hg_http_session.hpp"
#include "homeguard/access_control.hpp"
#include "esp_http_server.h"

#include <array>
#include <cstddef>
#include <string>

namespace homeguard::idf::request_auth {

inline bool read_header(httpd_req_t* request, std::string& value) {
    value.clear();
    if (request == nullptr) return false;
    const auto length = httpd_req_get_hdr_value_len(request, "Authorization");
    if (length == 0U || length > 96U) return false;
    std::array<char, 98> buffer{};
    if (httpd_req_get_hdr_value_str(request, "Authorization", buffer.data(), buffer.size()) != ESP_OK) return false;
    value.assign(buffer.data(), length);
    return true;
}

inline homeguard::AuditDecision authenticate(httpd_req_t* request, homeguard::AccessControl& access) {
    std::string authorization;
    if (!read_header(request, authorization)) return homeguard::AuditDecision::DeniedCredential;
    return http_session::authorized(authorization, access)
        ? homeguard::AuditDecision::Allowed
        : homeguard::AuditDecision::DeniedCredential;
}

inline bool authenticated(httpd_req_t* request, homeguard::AccessControl& access) {
    return authenticate(request, access) == homeguard::AuditDecision::Allowed;
}

inline esp_err_t send_login_required(httpd_req_t* request) {
    httpd_resp_set_status(request, "401 Unauthorized");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "WWW-Authenticate", "Bearer realm=\"homeguard\"");
    return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"login_required\"}", HTTPD_RESP_USE_STRLEN);
}

}  // namespace homeguard::idf::request_auth
