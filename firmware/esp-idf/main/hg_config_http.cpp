#include "hg_config_http.hpp"
#include "hg_access_time.hpp"

#include "esp_wifi.h"
#include "nvs.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace homeguard::idf {
namespace {

ConfigHttp* self_from(httpd_req_t* request) {
    return request == nullptr ? nullptr : static_cast<ConfigHttp*>(request->user_ctx);
}

bool read_body(httpd_req_t* request, std::size_t limit, std::string& body) {
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

bool parse_json_string(const std::string& body, const char* key, std::string& value) {
    const std::string marker = std::string{"\""} + key + "\"";
    auto pos = body.find(marker);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + marker.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
    if (pos >= body.size() || body[pos] != '"') return false;
    ++pos;
    value.clear();
    bool escaped = false;
    for (; pos < body.size(); ++pos) {
        const char ch = body[pos];
        if (escaped) {
            switch (ch) {
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                default: return false;
            }
            escaped = false;
            continue;
        }
        if (ch == '\\') { escaped = true; continue; }
        if (ch == '"') return true;
        value.push_back(ch);
    }
    return false;
}

bool parse_json_bool(const std::string& body, const char* key, bool& value) {
    const std::string marker = std::string{"\""} + key + "\"";
    auto pos = body.find(marker);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + marker.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
    if (body.compare(pos, 4, "true") == 0) { value = true; return true; }
    if (body.compare(pos, 5, "false") == 0) { value = false; return true; }
    return false;
}

bool parse_json_u32(const std::string& body, const char* key, std::uint32_t& value) {
    const std::string marker = std::string{"\""} + key + "\"";
    auto pos = body.find(marker);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + marker.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
    if (pos >= body.size() || !std::isdigit(static_cast<unsigned char>(body[pos]))) return false;
    std::uint32_t result = 0;
    while (pos < body.size() && std::isdigit(static_cast<unsigned char>(body[pos]))) {
        result = result * 10U + static_cast<std::uint32_t>(body[pos] - '0');
        ++pos;
    }
    value = result;
    return true;
}

std::string json_escape(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 8U);
    for (const unsigned char ch : value) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: if (ch >= 0x20U) out.push_back(static_cast<char>(ch)); break;
        }
    }
    return out;
}

esp_err_t send_json(httpd_req_t* request, const std::string& body) {
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

std::string current_sta_ssid() {
    wifi_config_t config{};
    if (esp_wifi_get_config(WIFI_IF_STA, &config) != ESP_OK) return {};
    std::size_t length = 0;
    while (length < sizeof(config.sta.ssid) && config.sta.ssid[length] != 0U) ++length;
    return std::string(reinterpret_cast<const char*>(config.sta.ssid), length);
}

}  // namespace

esp_err_t ConfigHttp::register_handlers(httpd_handle_t server,
                                        AccessControl* access,
                                        NetworkHttp* network,
                                        CloudNvsStore* cloud_store) {
    if (server == nullptr || access == nullptr || network == nullptr || cloud_store == nullptr) return ESP_ERR_INVALID_ARG;
    access_ = access;
    network_ = network;
    cloud_store_ = cloud_store;
    const httpd_uri_t route{
        .uri = "/api/v1/config",
        .method = HTTP_POST,
        .handler = &ConfigHttp::config_post,
        .user_ctx = this,
    };
    return httpd_register_uri_handler(server, &route);
}

esp_err_t ConfigHttp::config_post(httpd_req_t* request) {
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_config(request);
}

esp_err_t ConfigHttp::handle_config(httpd_req_t* request) {
    std::string body;
    if (!read_body(request, 4096U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }

    std::string action;
    std::string actor;
    std::string credential;
    if (!parse_json_string(body, "action", action) ||
        !parse_json_string(body, "actor", actor) ||
        !parse_json_string(body, "credential", credential)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }

    const auto decision = access_->authorize(actor, credential, "system.config.manage", access_now_ms());
    if (decision != AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return send_json(request, std::string{"{\"ok\":false,\"reason\":\""} + to_string(decision) + "\"}");
    }

    if (action == "export") return handle_export(request);
    if (action == "import") return handle_import(request, body);
    httpd_resp_set_status(request, "400 Bad Request");
    return send_json(request, "{\"ok\":false,\"reason\":\"unknown_action\"}");
}

esp_err_t ConfigHttp::handle_export(httpd_req_t* request) {
    CloudConfig cloud{};
    const auto cloud_error = cloud_store_->load(cloud);
    if (cloud_error != ESP_OK && cloud_error != ESP_ERR_NVS_NOT_FOUND) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return send_json(request, "{\"ok\":false,\"reason\":\"cloud_nvs_read_failed\"}");
    }

    std::string response = "{\"ok\":true,\"schema\":\"homeguard.config\",\"version\":1,\"secretsExcluded\":true";
    response += ",\"wifiSsid\":\"" + json_escape(current_sta_ssid()) + "\"";
    response += ",\"cloudEnabled\":" + std::string(cloud.enabled ? "true" : "false");
    response += ",\"cloudBrokerUri\":\"" + json_escape(cloud.broker_uri) + "\"";
    response += ",\"cloudUsername\":\"" + json_escape(cloud.username) + "\"";
    response += ",\"users\":[";
    for (std::size_t i = 0; i < access_->user_count(); ++i) {
        const auto* user = access_->user_at(i);
        if (user == nullptr) continue;
        if (i != 0U) response.push_back(',');
        response += "{\"id\":\"" + json_escape(user->id.data()) +
                    "\",\"name\":\"" + json_escape(user->name.data()) +
                    "\",\"role\":\"" + to_string(user->role) +
                    "\",\"enabled\":" + (user->enabled ? "true" : "false") + "}";
    }
    response += "]}";
    std::fill(cloud.password.begin(), cloud.password.end(), '\0');
    return send_json(request, response);
}

esp_err_t ConfigHttp::handle_import(httpd_req_t* request, const std::string& body) {
    std::uint32_t version = 0;
    if (!parse_json_u32(body, "version", version) || version != 1U) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"unsupported_version\"}");
    }

    std::string wifi_ssid;
    std::string wifi_password;
    std::string broker_uri;
    std::string username;
    std::string cloud_password;
    bool cloud_enabled = false;
    const bool has_wifi_ssid = parse_json_string(body, "wifiSsid", wifi_ssid);
    const bool has_wifi_password = parse_json_string(body, "wifiPassword", wifi_password);
    const bool has_cloud_enabled = parse_json_bool(body, "cloudEnabled", cloud_enabled);
    const bool has_broker = parse_json_string(body, "cloudBrokerUri", broker_uri);
    const bool has_username = parse_json_string(body, "cloudUsername", username);
    const bool has_cloud_password = parse_json_string(body, "cloudPassword", cloud_password);

    if ((has_wifi_ssid != has_wifi_password) ||
        (has_wifi_ssid && (wifi_ssid.empty() || wifi_ssid.size() > 32U || wifi_password.size() > 63U))) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_wifi_config\"}");
    }

    CloudConfig previous_cloud{};
    const auto previous_error = cloud_store_->load(previous_cloud);
    const bool had_previous_cloud = previous_error == ESP_OK;
    if (previous_error != ESP_OK && previous_error != ESP_ERR_NVS_NOT_FOUND) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return send_json(request, "{\"ok\":false,\"reason\":\"cloud_nvs_read_failed\"}");
    }

    CloudConfig next_cloud = previous_cloud;
    if (has_cloud_enabled) next_cloud.enabled = cloud_enabled;
    if (has_broker) next_cloud.broker_uri = broker_uri;
    if (has_username) next_cloud.username = username;
    if (has_cloud_password) next_cloud.password = cloud_password;
    if (next_cloud.broker_uri.size() > 255U || next_cloud.username.size() > 96U || next_cloud.password.size() > 128U) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_cloud_config\"}");
    }

    const bool cloud_changed = has_cloud_enabled || has_broker || has_username || has_cloud_password;
    if (cloud_changed && cloud_store_->save(next_cloud) != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return send_json(request, "{\"ok\":false,\"reason\":\"cloud_nvs_write_failed\"}");
    }

    if (has_wifi_ssid && !network_->restore_credentials(wifi_ssid, wifi_password)) {
        if (cloud_changed) {
            if (had_previous_cloud) (void)cloud_store_->save(previous_cloud);
            else (void)cloud_store_->clear();
        }
        httpd_resp_set_status(request, "500 Internal Server Error");
        return send_json(request, "{\"ok\":false,\"reason\":\"wifi_nvs_write_failed\",\"rolledBack\":true}");
    }

    std::fill(wifi_password.begin(), wifi_password.end(), '\0');
    std::fill(cloud_password.begin(), cloud_password.end(), '\0');
    std::fill(previous_cloud.password.begin(), previous_cloud.password.end(), '\0');
    std::fill(next_cloud.password.begin(), next_cloud.password.end(), '\0');
    return send_json(request, "{\"ok\":true,\"version\":1,\"restartRequired\":true}");
}

}  // namespace homeguard::idf
