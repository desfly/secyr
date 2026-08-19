#include "hg_network_http.hpp"
#include "hg_access_runtime.hpp"
#include "hg_request_auth.hpp"

#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace homeguard::idf {
namespace {

constexpr std::uint32_t kCredentialsMagic = 0x48475746U; // HGWF
constexpr char kNvsNamespace[] = "hg_wifi";
constexpr char kNvsKey[] = "credentials";
constexpr std::size_t kMaxScanRecords = 20;
constexpr TickType_t kStaHandoverDelay = pdMS_TO_TICKS(400);

struct CredentialsRecord {
    std::uint32_t magic{};
    std::uint8_t ssid_length{};
    std::uint8_t password_length{};
    std::array<char, 32> ssid{};
    std::array<char, 64> password{};
};

NetworkHttp* self_from(httpd_req_t* request)
{
    return request == nullptr ? nullptr : static_cast<NetworkHttp*>(request->user_ctx);
}

std::string json_escape(const std::string& value)
{
    std::string out;
    out.reserve(value.size() + 8);
    for (const unsigned char ch : value) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch >= 0x20U) out.push_back(static_cast<char>(ch));
                break;
        }
    }
    return out;
}

bool parse_json_string(const std::string& body, const char* key, std::string& value)
{
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
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') return true;
        value.push_back(ch);
    }
    return false;
}

std::string ssid_from_bytes(const std::uint8_t* bytes, std::size_t capacity)
{
    std::size_t length = 0;
    while (length < capacity && bytes[length] != 0) ++length;
    return std::string(reinterpret_cast<const char*>(bytes), length);
}

esp_err_t send_json(httpd_req_t* request, const std::string& body)
{
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

bool setup_network_allowed(const NetworkHttp* self)
{
    return self != nullptr && self->access_control() != nullptr &&
           access_runtime::setup_required(*self->access_control());
}

}  // namespace

esp_err_t NetworkHttp::begin()
{
    if (initialized_) return ESP_OK;

    auto* ap_netif = esp_netif_create_default_wifi_ap();
    auto* sta_netif = esp_netif_create_default_wifi_sta();
    if (ap_netif == nullptr || sta_netif == nullptr) return ESP_FAIL;
    sta_netif_ = sta_netif;

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    auto error = esp_wifi_init(&init);
    if (error != ESP_OK) return error;

    error = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (error != ESP_OK) return error;

    std::uint8_t mac[6]{};
    if (esp_wifi_get_mac(WIFI_IF_AP, mac) == ESP_OK) {
        char suffix[5]{};
        std::snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
        ap_ssid_ = std::string{"HomeGuard-S3-"} + suffix;
    } else {
        ap_ssid_ = "HomeGuard-S3-Setup";
    }

    wifi_config_t ap{};
    const auto ap_length = std::min(ap_ssid_.size(), sizeof(ap.ap.ssid));
    std::memcpy(ap.ap.ssid, ap_ssid_.data(), ap_length);
    ap.ap.ssid_len = static_cast<std::uint8_t>(ap_length);
    ap.ap.channel = 1;
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    error = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (error != ESP_OK) return error;

    error = esp_wifi_start();
    if (error != ESP_OK) return error;

    initialized_ = true;

    std::string ssid;
    std::string password;
    if (load_credentials(ssid, password)) {
        (void)apply_sta(ssid, password, false);
        std::fill(password.begin(), password.end(), '\0');
    }

    return ESP_OK;
}

esp_err_t NetworkHttp::register_handlers(httpd_handle_t server)
{
    if (server == nullptr || !initialized_ || access_ == nullptr) return ESP_ERR_INVALID_ARG;

    const httpd_uri_t routes[] = {
        {.uri="/api/v1/network/status", .method=HTTP_GET, .handler=&NetworkHttp::status_get, .user_ctx=this},
        {.uri="/api/v1/network/scan", .method=HTTP_GET, .handler=&NetworkHttp::scan_get, .user_ctx=this},
        {.uri="/api/v1/network/connect", .method=HTTP_POST, .handler=&NetworkHttp::connect_post, .user_ctx=this},
    };

    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

esp_err_t NetworkHttp::status_get(httpd_req_t* request)
{
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_status(request);
}

esp_err_t NetworkHttp::scan_get(httpd_req_t* request)
{
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_scan(request);
}

esp_err_t NetworkHttp::connect_post(httpd_req_t* request)
{
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_connect(request);
}

esp_err_t NetworkHttp::handle_status(httpd_req_t* request)
{
    if (!setup_network_allowed(this) && !request_auth::authenticated(request, *access_)) {
        return request_auth::send_login_required(request);
    }
    return send_json(request, status_json());
}

esp_err_t NetworkHttp::handle_scan(httpd_req_t* request)
{
    if (!setup_network_allowed(this) && !request_auth::authenticated(request, *access_)) {
        return request_auth::send_login_required(request);
    }
    return send_json(request, scan_json());
}

esp_err_t NetworkHttp::handle_connect(httpd_req_t* request)
{
    if (request->content_len == 0 || request->content_len > 384) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"state\":\"error\",\"reason\":\"invalid_body\"}");
    }

    std::string body(request->content_len, '\0');
    std::size_t offset = 0U;
    while (offset < body.size()) {
        const auto received = httpd_req_recv(request, body.data() + offset, body.size() - offset);
        if (received <= 0) {
            std::fill(body.begin(), body.end(), '\0');
            httpd_resp_set_status(request, "400 Bad Request");
            return send_json(request, "{\"ok\":false,\"state\":\"error\",\"reason\":\"read_failed\"}");
        }
        offset += static_cast<std::size_t>(received);
    }

    const bool setup_mode = setup_network_allowed(this);
    if (!setup_mode) {
        std::string actor;
        if (!parse_json_string(body, "actor", actor) || actor.empty()) {
            std::fill(body.begin(), body.end(), '\0');
            httpd_resp_set_status(request, "401 Unauthorized");
            return send_json(request, "{\"ok\":false,\"state\":\"error\",\"reason\":\"session_actor_required\"}");
        }
        if (!request_auth::authenticated_actor(request, *access_, actor)) {
            std::fill(body.begin(), body.end(), '\0');
            return request_auth::send_login_required(request);
        }

        const auto decision = access_->authorize_session(actor, "network.configure");
        if (decision != AuditDecision::Allowed) {
            std::fill(body.begin(), body.end(), '\0');
            httpd_resp_set_status(request, "403 Forbidden");
            return send_json(request, std::string{"{\"ok\":false,\"state\":\"error\",\"reason\":\""} + to_string(decision) + "\"}");
        }

        /* LEGACY v1 disabled: actor + credential used to be parsed and PIN
           re-checked here for every Wi-Fi configuration request. */
    }

    std::string ssid;
    std::string password;
    if (!parse_json_string(body, "ssid", ssid) || !parse_json_string(body, "password", password) ||
        ssid.empty() || ssid.size() > 32 || password.size() > 64 || (!password.empty() && password.size() < 8)) {
        std::fill(password.begin(), password.end(), '\0');
        std::fill(body.begin(), body.end(), '\0');
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"state\":\"error\",\"reason\":\"invalid_credentials\"}");
    }
    std::fill(body.begin(), body.end(), '\0');

    wifi_config_t sta{};
    std::memcpy(sta.sta.ssid, ssid.data(), std::min(ssid.size(), sizeof(sta.sta.ssid)));
    std::memcpy(sta.sta.password, password.data(), std::min(password.size(), sizeof(sta.sta.password)));
    sta.sta.threshold.authmode = WIFI_AUTH_OPEN;
    sta.sta.pmf_cfg.capable = true;
    sta.sta.pmf_cfg.required = false;

    wifi_config_t previous_sta{};
    const bool have_previous_sta = esp_wifi_get_config(WIFI_IF_STA, &previous_sta) == ESP_OK;
    std::string previous_ssid;
    std::string previous_password;
    const bool had_persisted_credentials = load_credentials(previous_ssid, previous_password);

    if (esp_wifi_set_config(WIFI_IF_STA, &sta) != ESP_OK) {
        std::fill(password.begin(), password.end(), '\0');
        std::fill(previous_password.begin(), previous_password.end(), '\0');
        httpd_resp_set_status(request, "503 Service Unavailable");
        return send_json(request, "{\"ok\":false,\"state\":\"error\",\"reason\":\"wifi_connect_failed\"}");
    }
    if (!save_credentials(ssid, password)) {
        if (have_previous_sta) (void)esp_wifi_set_config(WIFI_IF_STA, &previous_sta);
        std::fill(password.begin(), password.end(), '\0');
        std::fill(previous_password.begin(), previous_password.end(), '\0');
        httpd_resp_set_status(request, "503 Service Unavailable");
        return send_json(request, "{\"ok\":false,\"state\":\"error\",\"reason\":\"wifi_persist_failed\"}");
    }

    vTaskDelay(kStaHandoverDelay);
    (void)esp_wifi_disconnect();
    const auto connect_error = esp_wifi_connect();
    if (connect_error != ESP_OK) {
        if (have_previous_sta) (void)esp_wifi_set_config(WIFI_IF_STA, &previous_sta);
        const bool persist_rollback_ok = had_persisted_credentials
            ? save_credentials(previous_ssid, previous_password)
            : clear_credentials();
        if (have_previous_sta) {
            (void)esp_wifi_disconnect();
            (void)esp_wifi_connect();
        }
        std::fill(password.begin(), password.end(), '\0');
        std::fill(previous_password.begin(), previous_password.end(), '\0');
        httpd_resp_set_status(request, "503 Service Unavailable");
        return send_json(request, persist_rollback_ok
            ? "{\"ok\":false,\"state\":\"error\",\"reason\":\"wifi_connect_failed\",\"rolledBack\":true}"
            : "{\"ok\":false,\"state\":\"error\",\"reason\":\"wifi_connect_failed_rollback_failed\"}");
    }

    std::fill(password.begin(), password.end(), '\0');
    std::fill(previous_password.begin(), previous_password.end(), '\0');
    return send_json(request,
        std::string{"{\"ok\":true,\"state\":\"connecting\",\"ssid\":\""} + json_escape(ssid) +
        "\",\"setupMode\":" + (setup_mode ? "true" : "false") + "}");
}

bool NetworkHttp::apply_sta(const std::string& ssid, const std::string& password, bool persist)
{
    if (!initialized_ || ssid.empty() || ssid.size() > 32 || password.size() > 64 ||
        (!password.empty() && password.size() < 8)) return false;

    wifi_config_t sta{};
    std::memcpy(sta.sta.ssid, ssid.data(), std::min(ssid.size(), sizeof(sta.sta.ssid)));
    std::memcpy(sta.sta.password, password.data(), std::min(password.size(), sizeof(sta.sta.password)));
    sta.sta.threshold.authmode = WIFI_AUTH_OPEN;
    sta.sta.pmf_cfg.capable = true;
    sta.sta.pmf_cfg.required = false;

    wifi_config_t previous_sta{};
    const bool have_previous_sta = persist && esp_wifi_get_config(WIFI_IF_STA, &previous_sta) == ESP_OK;
    if (esp_wifi_set_config(WIFI_IF_STA, &sta) != ESP_OK) return false;
    if (persist && !save_credentials(ssid, password)) {
        if (have_previous_sta) (void)esp_wifi_set_config(WIFI_IF_STA, &previous_sta);
        return false;
    }

    (void)esp_wifi_disconnect();
    return esp_wifi_connect() == ESP_OK;
}

bool NetworkHttp::load_credentials(std::string& ssid, std::string& password) const
{
    nvs_handle_t handle{};
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) return false;

    CredentialsRecord record{};
    std::size_t size = sizeof(record);
    const auto error = nvs_get_blob(handle, kNvsKey, &record, &size);
    nvs_close(handle);
    if (error != ESP_OK || size != sizeof(record) || record.magic != kCredentialsMagic ||
        record.ssid_length == 0 || record.ssid_length > record.ssid.size() ||
        record.password_length > record.password.size()) return false;

    ssid.assign(record.ssid.data(), record.ssid_length);
    password.assign(record.password.data(), record.password_length);
    return true;
}

bool NetworkHttp::save_credentials(const std::string& ssid, const std::string& password) const
{
    CredentialsRecord record{};
    record.magic = kCredentialsMagic;
    record.ssid_length = static_cast<std::uint8_t>(ssid.size());
    record.password_length = static_cast<std::uint8_t>(password.size());
    std::memcpy(record.ssid.data(), ssid.data(), ssid.size());
    std::memcpy(record.password.data(), password.data(), password.size());

    nvs_handle_t handle{};
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
    const auto set_error = nvs_set_blob(handle, kNvsKey, &record, sizeof(record));
    const auto commit_error = set_error == ESP_OK ? nvs_commit(handle) : set_error;
    nvs_close(handle);
    return set_error == ESP_OK && commit_error == ESP_OK;
}

bool NetworkHttp::clear_credentials() const
{
    nvs_handle_t handle{};
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
    auto erase_error = nvs_erase_key(handle, kNvsKey);
    if (erase_error == ESP_ERR_NVS_NOT_FOUND) erase_error = ESP_OK;
    const auto commit_error = erase_error == ESP_OK ? nvs_commit(handle) : erase_error;
    nvs_close(handle);
    return erase_error == ESP_OK && commit_error == ESP_OK;
}

std::string NetworkHttp::status_json() const
{
    wifi_ap_record_t ap_info{};
    const bool connected = esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;

    wifi_config_t sta{};
    std::string ssid;
    if (connected) {
        ssid = ssid_from_bytes(ap_info.ssid, sizeof(ap_info.ssid));
    } else if (esp_wifi_get_config(WIFI_IF_STA, &sta) == ESP_OK) {
        ssid = ssid_from_bytes(sta.sta.ssid, sizeof(sta.sta.ssid));
    }

    std::string ip;
    if (connected && sta_netif_ != nullptr) {
        esp_netif_ip_info_t info{};
        if (esp_netif_get_ip_info(static_cast<esp_netif_t*>(sta_netif_), &info) == ESP_OK) {
            char address[16]{};
            esp_ip4addr_ntoa(&info.ip, address, sizeof(address));
            ip = address;
        }
    }

    const char* state = connected ? "connected" : (ssid.empty() ? "idle" : "connecting");
    std::string out = std::string{"{\"ok\":true,\"state\":\""} + state +
        "\",\"ssid\":\"" + json_escape(ssid) +
        "\",\"ip\":\"" + json_escape(ip) +
        "\",\"apSsid\":\"" + json_escape(ap_ssid_) + "\"";
    if (connected) out += ",\"rssi\":" + std::to_string(static_cast<int>(ap_info.rssi));
    out += '}';
    return out;
}

std::string NetworkHttp::scan_json() const
{
    wifi_ap_record_t current_ap{};
    const bool sta_was_connected = esp_wifi_sta_get_ap_info(&current_ap) == ESP_OK;
    if (sta_was_connected) {
        (void)esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    const auto start_error = esp_wifi_scan_start(nullptr, true);
    if (start_error != ESP_OK) {
        if (sta_was_connected) (void)esp_wifi_connect();
        return "{\"ok\":false,\"state\":\"error\",\"reason\":\"scan_start_failed\",\"networks\":[]}";
    }

    std::uint16_t count = 0;
    if (esp_wifi_scan_get_ap_num(&count) != ESP_OK) {
        if (sta_was_connected) (void)esp_wifi_connect();
        return "{\"ok\":false,\"state\":\"error\",\"reason\":\"scan_count_failed\",\"networks\":[]}";
    }

    count = static_cast<std::uint16_t>(std::min<std::size_t>(count, kMaxScanRecords));
    std::array<wifi_ap_record_t, kMaxScanRecords> records{};
    std::uint16_t returned = count;
    if (returned != 0 && esp_wifi_scan_get_ap_records(&returned, records.data()) != ESP_OK) {
        if (sta_was_connected) (void)esp_wifi_connect();
        return "{\"ok\":false,\"state\":\"error\",\"reason\":\"scan_records_failed\",\"networks\":[]}";
    }

    std::string out = "{\"ok\":true,\"networks\":[";
    for (std::uint16_t i = 0; i < returned; ++i) {
        if (i != 0) out += ',';
        const auto ssid = ssid_from_bytes(records[i].ssid, sizeof(records[i].ssid));
        out += "{\"ssid\":\"" + json_escape(ssid) + "\",\"rssi\":" +
               std::to_string(static_cast<int>(records[i].rssi)) + "}";
    }
    out += "]}";

    if (sta_was_connected) (void)esp_wifi_connect();
    return out;
}

}  // namespace homeguard::idf
