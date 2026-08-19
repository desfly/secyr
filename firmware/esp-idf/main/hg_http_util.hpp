#pragma once

#include "esp_http_server.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>

namespace homeguard::idf::http_util {

inline std::size_t value_offset(const std::string& body, const char* key) {
    const std::string marker = std::string{"\""} + key + "\"";
    auto pos = body.find(marker);
    if (pos == std::string::npos) return std::string::npos;
    pos = body.find(':', pos + marker.size());
    if (pos == std::string::npos) return std::string::npos;
    ++pos;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
    return pos;
}

inline bool parse_json_string(const std::string& body, const char* key, std::string& value) {
    auto pos = value_offset(body, key);
    if (pos == std::string::npos || pos >= body.size() || body[pos] != '"') return false;
    ++pos;
    value.clear();
    bool escaped = false;
    for (; pos < body.size(); ++pos) {
        const char ch = body[pos];
        if (escaped) {
            if (ch == '"' || ch == '\\' || ch == '/') value.push_back(ch);
            else if (ch == 'n') value.push_back('\n');
            else if (ch == 'r') value.push_back('\r');
            else if (ch == 't') value.push_back('\t');
            else return false;
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == '"') {
            return true;
        } else {
            value.push_back(ch);
        }
    }
    return false;
}

inline bool read_body(httpd_req_t* request, std::size_t limit, std::string& body) {
    if (request == nullptr || request->content_len == 0 || request->content_len > limit) return false;
    body.assign(request->content_len, '\0');
    std::size_t offset = 0;
    while (offset < body.size()) {
        const auto received = httpd_req_recv(request, body.data() + offset, body.size() - offset);
        if (received <= 0) return false;
        offset += static_cast<std::size_t>(received);
    }
    return true;
}

inline void scrub(std::string& secret) {
    std::fill(secret.begin(), secret.end(), '\0');
    secret.clear();
}

}  // namespace homeguard::idf::http_util
