#include "hg_network_http.hpp"

#include "esp_log.h"
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

constexpr const char* kTag = "hg_network";
constexpr std::uint32_t kCredentialsMagic = 0x48475746U; // HGWF
constexpr char kNvsNamespace[] = "hg_wifi";
constexpr char kNvsKey[] = "credentials";
constexpr std::size_t kMaxScanRecords = 20;
constexpr TickType_t kStaHandoverDelay = pdMS_TO_TICKS(400);
constexpr std::uint64_t kReconnectInitialUs = 500000ULL;
constexpr std::uint64_t kReconnectMaximumUs = 30000000ULL;

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
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

}  // namespace

esp_err_t NetworkHttp::begin()
{
    if (initialized_) return ESP_OK;

    auto* ap_netif = esp_netif_create_default_wifi_ap();
    auto* sta_netif = esp_netif_create_default_wifi_sta();
    if (ap_netif == nullptr || sta_netif == nullptr) {
        esp_netif_destroy_default_wifi(sta_netif);
        esp_netif_destroy_default_wifi(ap_netif);
        return ESP_FAIL;
    }

    bool wifi_initialized = false;
    bool wifi_handler_registered = false;
    bool ip_handler_registered = false;
    bool wifi_started = false;

    auto rollback = [&]() {
        clear_pending_credentials();
        if (reconnect_timer_ != nullptr) {
            if (esp_timer_is_active(reconnect_timer_)) {
                (void)esp_timer_stop(reconnect_timer_);
            }
            (void)esp_timer_delete(reconnect_timer_);
            reconnect_timer_ = nullptr;
        }
        if (wifi_started) {
            (void)esp_wifi_stop();
        }
        if (ip_handler_registered) {
            (void)esp_event_handler_unregister(
                IP_EVENT,
                IP_EVENT_STA_GOT_IP,
                &NetworkHttp::ip_event_handler);
        }
        if (wifi_handler_registered) {
            (void)esp_event_handler_unregister(
                WIFI_EVENT,
                ESP_EVENT_ANY_ID,
                &NetworkHttp::wifi_event_handler);
        }
        // Default Wi-Fi netifs own default Wi-Fi handlers/driver bindings and
        // must be destroyed with the matching ESP-IDF helper before deinit.
        esp_netif_destroy_default_wifi(sta_netif);
        esp_netif_destroy_default_wifi(ap_netif);
        sta_netif_ = nullptr;
        if (wifi_initialized) {
            (void)esp_wifi_deinit();
        }
    };

    sta_netif_ = sta_netif;

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    auto error = esp_wifi_init(&init);
    if (error != ESP_OK) {
        rollback();
        return error;
    }
    wifi_initialized = true;

    const esp_timer_create_args_t reconnect_timer_args{
        .callback = &NetworkHttp::reconnect_timer_handler,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "hg_wifi_reconnect",
        .skip_unhandled_events = true,
    };
    error = esp_timer_create(&reconnect_timer_args, &reconnect_timer_);
    if (error != ESP_OK) {
        rollback();
        return error;
    }

    error = esp_event_handler_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &NetworkHttp::wifi_event_handler,
        this);
    if (error != ESP_OK) {
        rollback();
        return error;
    }
    wifi_handler_registered = true;

    error = esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &NetworkHttp::ip_event_handler,
        this);
    if (error != ESP_OK) {
        rollback();
        return error;
    }
    ip_handler_registered = true;

    error = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (error != ESP_OK) {
        rollback();
        return error;
    }

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
    if (error != ESP_OK) {
        rollback();
        return error;
    }

    error = esp_wifi_start();
    if (error != ESP_OK) {
        rollback();
        return error;
    }
    wifi_started = true;

    initialized_ = true;

    std::string ssid;
    std::string password;
    if (load_credentials(ssid, password)) {
        sta_credentials_configured_ = true;
        (void)apply_sta(ssid, password);
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
        const auto route_error = httpd_register_uri_handler(server, &route);
        if (route_error != ESP_OK) return route_error;
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

void NetworkHttp::wifi_event_handler(
    void* context,
    esp_event_base_t,
    std::int32_t id,
    void* data)
{
    auto* self = static_cast<NetworkHttp*>(context);
    if (self != nullptr) {
        self->handle_wifi_event(id, data);
    }
}

void NetworkHttp::ip_event_handler(
    void* context,
    esp_event_base_t,
    std::int32_t id,
    void* data)
{
    auto* self = static_cast<NetworkHttp*>(context);
    if (self != nullptr) {
        self->handle_ip_event(id, data);
    }
}

void NetworkHttp::reconnect_timer_handler(void* context)
{
    auto* self = static_cast<NetworkHttp*>(context);
    if (self != nullptr) {
        self->handle_reconnect_timer();
    }
}

void NetworkHttp::schedule_reconnect()
{
    if (!sta_credentials_configured_ || reconnect_timer_ == nullptr) {
        return;
    }

    if (reconnect_count_ == 0U) {
        reconnect_count_ = 1U;
    }

    const auto shift = std::min<std::uint32_t>(reconnect_count_ - 1U, 6U);
    const auto delay_us = std::min<std::uint64_t>(
        kReconnectInitialUs << shift,
        kReconnectMaximumUs);

    if (esp_timer_is_active(reconnect_timer_)) {
        (void)esp_timer_stop(reconnect_timer_);
    }

    const auto error = esp_timer_start_once(reconnect_timer_, delay_us);
    if (error != ESP_OK) {
        ESP_LOGW(kTag,
                 "Unable to schedule STA reconnect attempt %lu: %s",
                 static_cast<unsigned long>(reconnect_count_),
                 esp_err_to_name(error));
    } else if (reconnect_count_ == 1U || (reconnect_count_ % 5U) == 0U) {
        ESP_LOGW(kTag,
                 "STA reconnect attempt %lu scheduled in %llu ms; recovery AP active",
                 static_cast<unsigned long>(reconnect_count_),
                 static_cast<unsigned long long>(delay_us / 1000ULL));
    }
}

void NetworkHttp::handle_reconnect_timer()
{
    if (!sta_credentials_configured_) {
        return;
    }

    const auto error = esp_wifi_connect();
    if (error != ESP_OK) {
        ++reconnect_count_;
        ESP_LOGW(kTag,
                 "STA reconnect attempt failed to start: %s",
                 esp_err_to_name(error));
        schedule_reconnect();
    }
}

void NetworkHttp::handle_wifi_event(std::int32_t id, void*)
{
    if (id != WIFI_EVENT_STA_DISCONNECTED || !sta_credentials_configured_) {
        return;
    }

    ++reconnect_count_;
    const auto mode_error = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (mode_error != ESP_OK) {
        ESP_LOGW(kTag,
                 "Unable to restore recovery AP after STA disconnect: %s",
                 esp_err_to_name(mode_error));
    }
    schedule_reconnect();
}

void NetworkHttp::clear_pending_credentials()
{
    if (!pending_password_.empty()) {
        std::fill(pending_password_.begin(), pending_password_.end(), '\0');
    }
    pending_password_.clear();
    pending_ssid_.clear();
    pending_credentials_ = false;
}

void NetworkHttp::handle_ip_event(std::int32_t id, void*)
{
    if (id != IP_EVENT_STA_GOT_IP) {
        return;
    }

    if (reconnect_timer_ != nullptr && esp_timer_is_active(reconnect_timer_)) {
        (void)esp_timer_stop(reconnect_timer_);
    }
    reconnect_count_ = 0;

    if (pending_credentials_) {
        wifi_ap_record_t current{};
        if (esp_wifi_sta_get_ap_info(&current) != ESP_OK) {
            ESP_LOGW(kTag, "STA got-IP arrived but candidate SSID cannot be verified; recovery AP stays active");
            (void)esp_wifi_set_mode(WIFI_MODE_APSTA);
            return;
        }

        const auto connected_ssid = ssid_from_bytes(current.ssid, sizeof(current.ssid));
        if (connected_ssid != pending_ssid_) {
            ESP_LOGW(kTag,
                     "STA got-IP SSID mismatch (%s != %s); candidate credentials not committed",
                     connected_ssid.c_str(),
                     pending_ssid_.c_str());
            (void)esp_wifi_set_mode(WIFI_MODE_APSTA);
            return;
        }

        if (!save_credentials(pending_ssid_, pending_password_)) {
            ESP_LOGE(kTag,
                     "STA candidate connected to %s but NVS commit failed; recovery AP stays active",
                     pending_ssid_.c_str());
            (void)esp_wifi_set_mode(WIFI_MODE_APSTA);
            return;
        }

        ESP_LOGI(kTag, "STA candidate %s verified by got-IP and committed to NVS", pending_ssid_.c_str());
        clear_pending_credentials();
    }

    const auto error = esp_wifi_set_mode(WIFI_MODE_STA);
    if (error != ESP_OK) {
        ESP_LOGW(kTag, "Unable to retire recovery AP after STA got IP: %s", esp_err_to_name(error));
    } else {
        ESP_LOGI(kTag, "STA got IP; recovery AP retired");
    }
}

esp_err_t NetworkHttp::handle_status(httpd_req_t* request)
{
    return send_json(request, status_json());
}

esp_err_t NetworkHttp::handle_scan(httpd_req_t* request)
{
    return send_json(request, scan_json());
}

esp_err_t NetworkHttp::handle_connect(httpd_req_t* request)
{
    if (request->content_len == 0 || request->content_len > 384) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"state\":\"error\",\"reason\":\"invalid_body\"}");
    }

    std::string body(request->content_len, '\0');
    const auto received = httpd_req_recv(request, body.data(), body.size());
    if (received <= 0) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"state\":\"error\",\"reason\":\"read_failed\"}");
    }
    body.resize(static_cast<std::size_t>(received));

    std::string actor;
    std::string credential;
    if (!parse_json_string(body, "actor", actor) || !parse_json_string(body, "credential", credential)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return send_json(request, "{\"ok\":false,\"state\":\"error\",\"reason\":\"admin_credential_required\"}");
    }

    const auto decision = access_->authorize(actor, credential, "network.configure");
    std::fill(credential.begin(), credential.end(), '\0');
    if (decision != AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return send_json(request, std::string{"{\"ok\":false,\"state\":\"error\",\"reason\":\""} + to_string(decision) + "\"}");
    }

    std::string ssid;
    std::string password;
    if (!parse_json_string(body, "ssid", ssid) || !parse_json_string(body, "password", password) ||
        ssid.empty() || ssid.size() > 32 || password.size() > 64 || (!password.empty() && password.size() < 8)) {
        std::fill(password.begin(), password.end(), '\0');
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"state\":\"error\",\"reason\":\"invalid_credentials\"}");
    }

    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK) {
        std::fill(password.begin(), password.end(), '\0');
        httpd_resp_set_status(request, "503 Service Unavailable");
        return send_json(request, "{\"ok\":false,\"state\":\"error\",\"reason\":\"wifi_mode_failed\"}");
    }

    wifi_config_t sta{};
    std::memcpy(sta.sta.ssid, ssid.data(), std::min(ssid.size(), sizeof(sta.sta.ssid)));
    std::memcpy(sta.sta.password, password.data(), std::min(password.size(), sizeof(sta.sta.password)));
    sta.sta.threshold.authmode = WIFI_AUTH_OPEN;
    sta.sta.pmf_cfg.capable = true;
    sta.sta.pmf_cfg.required = false;

    if (esp_wifi_set_config(WIFI_IF_STA, &sta) != ESP_OK) {
        std::fill(password.begin(), password.end(), '\0');
        httpd_resp_set_status(request, "503 Service Unavailable");
        return send_json(request, "{\"ok\":false,\"state\":\"error\",\"reason\":\"wifi_config_failed\"}");
    }

    // Stage candidate credentials only in RAM. The last known-good NVS record
    // remains untouched until the new STA has actually received an IP on the
    // requested SSID. A bad password therefore cannot destroy the recovery path.
    clear_pending_credentials();
    pending_ssid_ = ssid;
    pending_password_ = password;
    pending_credentials_ = true;
    std::fill(password.begin(), password.end(), '\0');
    sta_credentials_configured_ = true;
    reconnect_count_ = 0;

    const auto response_error = send_json(request,
        std::string{"{\"ok\":true,\"state\":\"connecting\",\"ssid\":\""} + json_escape(ssid) +
        "\",\"credentialsPending\":true}");
    if (response_error != ESP_OK) return response_error;

    vTaskDelay(kStaHandoverDelay);
    wifi_ap_record_t current{};
    if (esp_wifi_sta_get_ap_info(&current) == ESP_OK) {
        return esp_wifi_disconnect();
    }

    reconnect_count_ = 1U;
    schedule_reconnect();
    return ESP_OK;
}

bool NetworkHttp::apply_sta(const std::string& ssid, const std::string& password)
{
    if (!initialized_ || ssid.empty() || ssid.size() > 32 || password.size() > 64 ||
        (!password.empty() && password.size() < 8)) return false;

    wifi_config_t sta{};
    std::memcpy(sta.sta.ssid, ssid.data(), std::min(ssid.size(), sizeof(sta.sta.ssid)));
    std::memcpy(sta.sta.password, password.data(), std::min(password.size(), sizeof(sta.sta.password)));
    sta.sta.threshold.authmode = WIFI_AUTH_OPEN;
    sta.sta.pmf_cfg.capable = true;
    sta.sta.pmf_cfg.required = false;

    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK) return false;
    if (esp_wifi_set_config(WIFI_IF_STA, &sta) != ESP_OK) return false;

    sta_credentials_configured_ = true;
    wifi_ap_record_t current{};
    if (esp_wifi_sta_get_ap_info(&current) == ESP_OK) {
        return esp_wifi_disconnect() == ESP_OK;
    }

    const auto error = esp_wifi_connect();
    if (error != ESP_OK) {
        reconnect_count_ = std::max<std::uint32_t>(reconnect_count_, 1U);
        schedule_reconnect();
    }
    return error == ESP_OK;
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
        "\",\"apSsid\":\"" + json_escape(ap_ssid_) +
        "\",\"credentialsPending\":" + (pending_credentials_ ? "true" : "false") +
        ",\"reconnectCount\":" + std::to_string(reconnect_count_);
    if (connected) out += ",\"rssi\":" + std::to_string(static_cast<int>(ap_info.rssi));
    out += '}';
    return out;
}

std::string NetworkHttp::scan_json() const
{
    const auto start_error = esp_wifi_scan_start(nullptr, true);
    if (start_error != ESP_OK) {
        return "{\"ok\":false,\"state\":\"error\",\"reason\":\"scan_start_failed\",\"networks\":[]}";
    }

    std::uint16_t count = 0;
    if (esp_wifi_scan_get_ap_num(&count) != ESP_OK) {
        if (sta_credentials_configured_) (void)esp_wifi_connect();
        return "{\"ok\":false,\"state\":\"error\",\"reason\":\"scan_count_failed\",\"networks\":[]}";
    }

    count = static_cast<std::uint16_t>(std::min<std::size_t>(count, kMaxScanRecords));
    std::array<wifi_ap_record_t, kMaxScanRecords> records{};
    std::uint16_t returned = count;
    if (returned != 0 && esp_wifi_scan_get_ap_records(&returned, records.data()) != ESP_OK) {
        if (sta_credentials_configured_) (void)esp_wifi_connect();
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

    wifi_ap_record_t current{};
    if (sta_credentials_configured_ && esp_wifi_sta_get_ap_info(&current) != ESP_OK) {
        (void)esp_wifi_connect();
    }
    return out;
}

}  // namespace homeguard::idf
