#include "hg_network_http.hpp"
#include "hg_access_runtime.hpp"
#include "hg_request_auth.hpp"

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
#include <new>
#include <string>
#include <utility>

namespace homeguard::idf {
namespace {

constexpr const char* kTag = "hg_network";
constexpr std::uint32_t kCredentialsMagic = 0x48475746U;
constexpr char kNvsNamespace[] = "hg_wifi";
constexpr char kNvsKey[] = "credentials";
constexpr char kCandidateNvsKey[] = "candidate";
constexpr std::size_t kMaxScanRecords = 20;
constexpr TickType_t kStaHandoverDelay = pdMS_TO_TICKS(150);
constexpr TickType_t kCandidateConnectTimeout = pdMS_TO_TICKS(30000);
constexpr TickType_t kReconnectRetryDelay = pdMS_TO_TICKS(10000);
constexpr unsigned kAsyncTaskStackBytes = 6144;
constexpr unsigned kReconnectTaskStackBytes = 2048;
constexpr unsigned kTimeoutTaskStackBytes = 2048;
constexpr unsigned kWorkerTaskPriority = 4;
constexpr unsigned kImmediateReconnectAttempts = 5;
constexpr unsigned kCandidateReconnectAttempts = 3;

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
            default: if (ch >= 0x20U) out.push_back(static_cast<char>(ch)); break;
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
        if (ch == '\\') { escaped = true; continue; }
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

esp_err_t send_error(httpd_req_t* request, const char* status, const char* reason)
{
    httpd_resp_set_status(request, status);
    return send_json(request, std::string{"{\"ok\":false,\"state\":\"error\",\"reason\":\""} + reason + "\"}");
}

bool setup_network_allowed(const NetworkHttp* self)
{
    return self != nullptr && self->access_control() != nullptr && access_runtime::setup_required(*self->access_control());
}

void wipe(std::string& secret)
{
    std::fill(secret.begin(), secret.end(), '\0');
    secret.clear();
}

void wipe_record(CredentialsRecord& record)
{
    std::fill(record.password.begin(), record.password.end(), '\0');
    record.password_length = 0;
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
    error = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (error != ESP_OK) return error;
    error = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &NetworkHttp::wifi_event_entry, this);
    if (error != ESP_OK) return error;
    error = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &NetworkHttp::ip_event_entry, this);
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

    (void)clear_candidate_credentials();
    error = esp_wifi_start();
    if (error != ESP_OK) return error;
    initialized_ = true;

    std::string ssid;
    std::string password;
    if (load_credentials(ssid, password)) {
        if (!apply_sta(ssid, password, false)) {
            sta_state_.store(StaState::Error);
            schedule_reconnect_retry();
        }
        wipe(password);
    } else {
        sta_state_.store(StaState::Idle);
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
    if (!setup_network_allowed(this) && !request_auth::authenticated(request, *access_)) return request_auth::send_login_required(request);
    return send_json(request, status_json());
}

esp_err_t NetworkHttp::handle_scan(httpd_req_t* request)
{
    if (!setup_network_allowed(this) && !request_auth::authenticated(request, *access_)) return request_auth::send_login_required(request);
    return dispatch_async(request, AsyncOperation::Scan);
}

esp_err_t NetworkHttp::handle_connect(httpd_req_t* request)
{
    if (request->content_len == 0 || request->content_len > 384) return send_error(request, "400 Bad Request", "invalid_body");
    std::string body(request->content_len, '\0');
    std::size_t offset = 0;
    while (offset < body.size()) {
        const auto received = httpd_req_recv(request, body.data() + offset, body.size() - offset);
        if (received <= 0) { wipe(body); return send_error(request, "400 Bad Request", "read_failed"); }
        offset += static_cast<std::size_t>(received);
    }

    const bool setup_mode = setup_network_allowed(this);
    if (!setup_mode) {
        std::string actor;
        if (!parse_json_string(body, "actor", actor) || actor.empty()) {
            wipe(body);
            httpd_resp_set_status(request, "401 Unauthorized");
            return send_json(request, "{\"ok\":false,\"state\":\"error\",\"reason\":\"session_actor_required\"}");
        }
        if (!request_auth::authenticated_actor(request, *access_, actor)) { wipe(body); return request_auth::send_login_required(request); }
        const auto decision = access_->authorize_session(actor, "network.configure");
        if (decision != AuditDecision::Allowed) {
            wipe(body);
            httpd_resp_set_status(request, "403 Forbidden");
            return send_json(request, std::string{"{\"ok\":false,\"state\":\"error\",\"reason\":\""} + to_string(decision) + "\"}");
        }
    }

    std::string ssid;
    std::string password;
    if (!parse_json_string(body, "ssid", ssid) || !parse_json_string(body, "password", password) || ssid.empty() || ssid.size() > 32 || password.size() > 64 || (!password.empty() && password.size() < 8)) {
        wipe(password);
        wipe(body);
        return send_error(request, "400 Bad Request", "invalid_credentials");
    }
    wipe(body);
    return dispatch_async(request, AsyncOperation::Connect, setup_mode, std::move(ssid), std::move(password));
}

esp_err_t NetworkHttp::dispatch_async(httpd_req_t* request, AsyncOperation operation, bool setup_mode, std::string ssid, std::string password)
{
    if (candidate_pending_.load()) { wipe(password); return send_error(request, "409 Conflict", "network_operation_busy"); }
    bool expected = false;
    if (!async_busy_.compare_exchange_strong(expected, true)) { wipe(password); return send_error(request, "409 Conflict", "network_operation_busy"); }

    httpd_req_t* async_request = nullptr;
    const auto begin_error = httpd_req_async_handler_begin(request, &async_request);
    if (begin_error != ESP_OK || async_request == nullptr) {
        async_busy_.store(false);
        wipe(password);
        return send_error(request, "503 Service Unavailable", "async_request_failed");
    }

    auto* context = new (std::nothrow) AsyncContext{this, async_request, operation, setup_mode, std::move(ssid), std::move(password)};
    if (context == nullptr) {
        (void)send_error(async_request, "503 Service Unavailable", "out_of_memory");
        (void)httpd_req_async_handler_complete(async_request);
        async_busy_.store(false);
        return ESP_OK;
    }
    if (xTaskCreate(&NetworkHttp::async_task_entry, operation == AsyncOperation::Scan ? "hg_wifi_scan" : "hg_wifi_connect", kAsyncTaskStackBytes, context, kWorkerTaskPriority, nullptr) != pdPASS) {
        wipe(context->password);
        (void)send_error(async_request, "503 Service Unavailable", "worker_start_failed");
        (void)httpd_req_async_handler_complete(async_request);
        delete context;
        async_busy_.store(false);
    }
    return ESP_OK;
}

void NetworkHttp::async_task_entry(void* argument)
{
    auto* context = static_cast<AsyncContext*>(argument);
    if (context == nullptr || context->self == nullptr) { delete context; vTaskDelete(nullptr); return; }
    auto* self = context->self;
    self->process_async(*context);
    wipe(context->password);
    delete context;
    self->async_busy_.store(false);
    vTaskDelete(nullptr);
}

void NetworkHttp::reconnect_task_entry(void* argument)
{
    auto* self = static_cast<NetworkHttp*>(argument);
    if (self == nullptr) { vTaskDelete(nullptr); return; }
    vTaskDelay(kReconnectRetryDelay);
    if (!self->candidate_pending_.load() && !self->async_busy_.load() && self->sta_state_.load() == StaState::Error) {
        self->reconnect_attempts_.store(0);
        self->sta_state_.store(StaState::Connecting);
        if (esp_wifi_connect() != ESP_OK) self->sta_state_.store(StaState::Error);
    }
    self->reconnect_scheduled_.store(false);
    if (!self->candidate_pending_.load() && self->sta_state_.load() == StaState::Error) self->schedule_reconnect_retry();
    vTaskDelete(nullptr);
}

void NetworkHttp::candidate_timeout_task_entry(void* argument)
{
    auto* context = static_cast<CandidateTimeoutContext*>(argument);
    if (context == nullptr || context->self == nullptr) { delete context; vTaskDelete(nullptr); return; }
    auto* self = context->self;
    const auto generation = context->generation;
    delete context;
    vTaskDelay(kCandidateConnectTimeout);
    if (self->candidate_generation_.load() == generation && self->candidate_pending_.exchange(false)) self->cancel_candidate_and_restore_active("candidate_timeout");
    vTaskDelete(nullptr);
}

void NetworkHttp::process_async(AsyncContext& context)
{
    if (context.operation == AsyncOperation::Scan) process_scan(context); else process_connect(context);
}

void NetworkHttp::process_scan(AsyncContext& context)
{
    const auto scan_response_error = send_json(context.request, scan_json());
    if (scan_response_error != ESP_OK) ESP_LOGW(kTag, "Wi-Fi scan response failed: %s", esp_err_to_name(scan_response_error));
    const auto complete_error = httpd_req_async_handler_complete(context.request);
    if (complete_error != ESP_OK) ESP_LOGW(kTag, "Wi-Fi scan async completion failed: %s", esp_err_to_name(complete_error));
}

void NetworkHttp::process_connect(AsyncContext& context)
{
    if (!save_candidate_credentials(context.ssid, context.password)) {
        (void)send_error(context.request, "503 Service Unavailable", "wifi_candidate_persist_failed");
        (void)httpd_req_async_handler_complete(context.request);
        return;
    }

    wifi_config_t next_sta{};
    std::memcpy(next_sta.sta.ssid, context.ssid.data(), std::min(context.ssid.size(), sizeof(next_sta.sta.ssid)));
    std::memcpy(next_sta.sta.password, context.password.data(), std::min(context.password.size(), sizeof(next_sta.sta.password)));
    next_sta.sta.threshold.authmode = WIFI_AUTH_OPEN;
    next_sta.sta.pmf_cfg.capable = true;
    next_sta.sta.pmf_cfg.required = false;

    std::string previous_ssid;
    std::string previous_password;
    const bool had_persisted_credentials = load_credentials(previous_ssid, previous_password);
    const auto previous_state = sta_state_.load();
    const auto config_error = esp_wifi_set_config(WIFI_IF_STA, &next_sta);
    std::memset(next_sta.sta.password, 0, sizeof(next_sta.sta.password));
    if (config_error != ESP_OK) {
        (void)clear_candidate_credentials();
        wipe(previous_password);
        (void)send_error(context.request, "503 Service Unavailable", "wifi_config_failed");
        (void)httpd_req_async_handler_complete(context.request);
        return;
    }

    candidate_attempts_.store(0);
    reconnect_attempts_.store(0);
    last_disconnect_reason_.store(0);
    const auto generation = candidate_generation_.fetch_add(1) + 1U;
    candidate_pending_.store(true);
    sta_state_.store(StaState::Connecting);

    httpd_resp_set_status(context.request, "202 Accepted");
    const auto response_error = send_json(context.request, std::string{"{\"ok\":true,\"state\":\"handover_pending\",\"ssid\":\""} + json_escape(context.ssid) + "\",\"setupMode\":" + (context.setup_mode ? "true" : "false") + "}");
    const auto complete_error = httpd_req_async_handler_complete(context.request);
    if (response_error != ESP_OK || complete_error != ESP_OK) {
        candidate_pending_.store(false);
        (void)clear_candidate_credentials();
        (void)restore_active_config_only();
        sta_state_.store(previous_state);
        wipe(previous_password);
        return;
    }

    vTaskDelay(kStaHandoverDelay);
    suppress_disconnect_reconnect_.store(true);
    const auto disconnect_error = esp_wifi_disconnect();
    if (disconnect_error != ESP_OK) suppress_disconnect_reconnect_.store(false);
    const auto connect_error = esp_wifi_connect();
    if (connect_error != ESP_OK) {
        const bool persist_rollback_ok = had_persisted_credentials
            ? save_credentials(previous_ssid, previous_password)
            : clear_credentials();
        if (!persist_rollback_ok) ESP_LOGE(kTag, "Committed Wi-Fi credential rollback failed");
        std::fill(previous_password.begin(), previous_password.end(), '\0');
        previous_password.clear();
        if (candidate_pending_.exchange(false)) cancel_candidate_and_restore_active("connect_command_failed");
        return;
    }

    wipe(previous_password);
    if (!start_candidate_timeout(generation) && candidate_pending_.exchange(false)) cancel_candidate_and_restore_active("timeout_guard_failed");
}

bool NetworkHttp::start_candidate_timeout(std::uint32_t generation)
{
    auto* context = new (std::nothrow) CandidateTimeoutContext{this, generation};
    if (context == nullptr) return false;
    if (xTaskCreate(&NetworkHttp::candidate_timeout_task_entry, "hg_wifi_timeout", kTimeoutTaskStackBytes, context, kWorkerTaskPriority, nullptr) != pdPASS) {
        delete context;
        return false;
    }
    return true;
}

void NetworkHttp::cancel_candidate_and_restore_active(const char* reason)
{
    (void)clear_candidate_credentials();
    candidate_attempts_.store(0);
    restore_in_progress_.store(true);
    ESP_LOGW(kTag, "Wi-Fi candidate cancelled (%s); restoring committed network", reason);
    restore_active_connection();
}

bool NetworkHttp::restore_active_config_only()
{
    std::string ssid;
    std::string password;
    wifi_config_t sta{};
    if (!load_credentials(ssid, password)) return esp_wifi_set_config(WIFI_IF_STA, &sta) == ESP_OK;
    std::memcpy(sta.sta.ssid, ssid.data(), std::min(ssid.size(), sizeof(sta.sta.ssid)));
    std::memcpy(sta.sta.password, password.data(), std::min(password.size(), sizeof(sta.sta.password)));
    sta.sta.threshold.authmode = WIFI_AUTH_OPEN;
    sta.sta.pmf_cfg.capable = true;
    sta.sta.pmf_cfg.required = false;
    const auto error = esp_wifi_set_config(WIFI_IF_STA, &sta);
    std::memset(sta.sta.password, 0, sizeof(sta.sta.password));
    wipe(password);
    return error == ESP_OK;
}

void NetworkHttp::restore_active_connection()
{
    std::string ssid;
    std::string password;
    if (!load_credentials(ssid, password)) {
        wifi_config_t empty{};
        (void)esp_wifi_set_config(WIFI_IF_STA, &empty);
        suppress_disconnect_reconnect_.store(true);
        if (esp_wifi_disconnect() != ESP_OK) suppress_disconnect_reconnect_.store(false);
        sta_state_.store(StaState::Idle);
        restore_in_progress_.store(false);
        return;
    }

    wifi_config_t sta{};
    std::memcpy(sta.sta.ssid, ssid.data(), std::min(ssid.size(), sizeof(sta.sta.ssid)));
    std::memcpy(sta.sta.password, password.data(), std::min(password.size(), sizeof(sta.sta.password)));
    sta.sta.threshold.authmode = WIFI_AUTH_OPEN;
    sta.sta.pmf_cfg.capable = true;
    sta.sta.pmf_cfg.required = false;
    const auto config_error = esp_wifi_set_config(WIFI_IF_STA, &sta);
    std::memset(sta.sta.password, 0, sizeof(sta.sta.password));
    wipe(password);
    if (config_error != ESP_OK) {
        sta_state_.store(StaState::Error);
        restore_in_progress_.store(false);
        schedule_reconnect_retry();
        return;
    }

    reconnect_attempts_.store(0);
    sta_state_.store(StaState::Connecting);
    suppress_disconnect_reconnect_.store(true);
    if (esp_wifi_disconnect() != ESP_OK) suppress_disconnect_reconnect_.store(false);
    if (esp_wifi_connect() != ESP_OK) {
        sta_state_.store(StaState::Error);
        schedule_reconnect_retry();
    }
}

void NetworkHttp::wifi_event_entry(void* argument, esp_event_base_t, std::int32_t id, void* data)
{
    auto* self = static_cast<NetworkHttp*>(argument);
    if (self != nullptr) self->on_wifi_event(id, data);
}

void NetworkHttp::ip_event_entry(void* argument, esp_event_base_t, std::int32_t id, void* data)
{
    auto* self = static_cast<NetworkHttp*>(argument);
    if (self != nullptr) self->on_ip_event(id, data);
}

void NetworkHttp::on_wifi_event(std::int32_t id, void* data)
{
    if (id != WIFI_EVENT_STA_DISCONNECTED) return;
    const auto* disconnected = static_cast<wifi_event_sta_disconnected_t*>(data);
    last_disconnect_reason_.store(disconnected == nullptr ? 0 : static_cast<int>(disconnected->reason));
    if (suppress_disconnect_reconnect_.exchange(false)) return;

    if (candidate_pending_.load()) {
        const unsigned attempt = candidate_attempts_.fetch_add(1) + 1U;
        if (attempt > kCandidateReconnectAttempts) {
            if (candidate_pending_.exchange(false)) cancel_candidate_and_restore_active("candidate_retries_exhausted");
            return;
        }
        sta_state_.store(StaState::Connecting);
        if (esp_wifi_connect() != ESP_OK && candidate_pending_.exchange(false)) cancel_candidate_and_restore_active("candidate_retry_command_failed");
        return;
    }

    if (sta_state_.load() == StaState::Idle) return;
    const unsigned attempt = reconnect_attempts_.fetch_add(1) + 1U;
    if (attempt > kImmediateReconnectAttempts) {
        sta_state_.store(StaState::Error);
        schedule_reconnect_retry();
        return;
    }
    sta_state_.store(StaState::Connecting);
    if (esp_wifi_connect() != ESP_OK) {
        sta_state_.store(StaState::Error);
        schedule_reconnect_retry();
    }
}

void NetworkHttp::schedule_reconnect_retry()
{
    bool expected = false;
    if (!reconnect_scheduled_.compare_exchange_strong(expected, true)) return;
    if (xTaskCreate(&NetworkHttp::reconnect_task_entry, "hg_wifi_retry", kReconnectTaskStackBytes, this, kWorkerTaskPriority, nullptr) != pdPASS) {
        reconnect_scheduled_.store(false);
        ESP_LOGE(kTag, "Unable to start delayed Wi-Fi reconnect worker");
    }
}

void NetworkHttp::on_ip_event(std::int32_t id, void*)
{
    if (id != IP_EVENT_STA_GOT_IP) return;

    if (candidate_pending_.exchange(false)) {
        std::string candidate_ssid;
        std::string candidate_password;
        const bool candidate_loaded = load_candidate_credentials(candidate_ssid, candidate_password);
        const bool committed = candidate_loaded && save_credentials(candidate_ssid, candidate_password);
        wipe(candidate_password);
        (void)clear_candidate_credentials();
        if (!committed) {
            restore_in_progress_.store(true);
            restore_active_connection();
            return;
        }
        restore_in_progress_.store(false);
        candidate_attempts_.store(0);
        reconnect_attempts_.store(0);
        last_disconnect_reason_.store(0);
        sta_state_.store(StaState::Connected);
        ESP_LOGI(kTag, "Wi-Fi candidate connected and committed: %s", candidate_ssid.c_str());
        return;
    }

    if (restore_in_progress_.load()) {
        std::string active_ssid;
        std::string active_password;
        const bool have_active = load_credentials(active_ssid, active_password);
        wipe(active_password);
        wifi_ap_record_t ap_info{};
        const bool associated = esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
        const std::string actual_ssid = associated ? ssid_from_bytes(ap_info.ssid, sizeof(ap_info.ssid)) : std::string{};
        if (!have_active || actual_ssid != active_ssid) return;
        restore_in_progress_.store(false);
    }

    reconnect_attempts_.store(0);
    last_disconnect_reason_.store(0);
    sta_state_.store(StaState::Connected);
}

bool NetworkHttp::apply_sta(const std::string& ssid, const std::string& password, bool persist)
{
    if (!initialized_ || ssid.empty() || ssid.size() > 32 || password.size() > 64 || (!password.empty() && password.size() < 8)) return false;
    if (persist && !save_credentials(ssid, password)) return false;
    wifi_config_t sta{};
    std::memcpy(sta.sta.ssid, ssid.data(), std::min(ssid.size(), sizeof(sta.sta.ssid)));
    std::memcpy(sta.sta.password, password.data(), std::min(password.size(), sizeof(sta.sta.password)));
    sta.sta.threshold.authmode = WIFI_AUTH_OPEN;
    sta.sta.pmf_cfg.capable = true;
    sta.sta.pmf_cfg.required = false;
    const auto config_error = esp_wifi_set_config(WIFI_IF_STA, &sta);
    std::memset(sta.sta.password, 0, sizeof(sta.sta.password));
    if (config_error != ESP_OK) return false;
    reconnect_attempts_.store(0);
    last_disconnect_reason_.store(0);
    sta_state_.store(StaState::Connecting);
    suppress_disconnect_reconnect_.store(true);
    if (esp_wifi_disconnect() != ESP_OK) suppress_disconnect_reconnect_.store(false);
    return esp_wifi_connect() == ESP_OK;
}

bool NetworkHttp::load_credentials(std::string& ssid, std::string& password) const { return load_credentials_from_key(kNvsKey, ssid, password); }
bool NetworkHttp::save_credentials(const std::string& ssid, const std::string& password) const { return save_credentials_to_key(kNvsKey, ssid, password); }
bool NetworkHttp::clear_credentials() const { return clear_credentials_key(kNvsKey); }
bool NetworkHttp::load_candidate_credentials(std::string& ssid, std::string& password) const { return load_credentials_from_key(kCandidateNvsKey, ssid, password); }
bool NetworkHttp::save_candidate_credentials(const std::string& ssid, const std::string& password) const { return save_credentials_to_key(kCandidateNvsKey, ssid, password); }
bool NetworkHttp::clear_candidate_credentials() const { return clear_credentials_key(kCandidateNvsKey); }

bool NetworkHttp::load_credentials_from_key(const char* key, std::string& ssid, std::string& password) const
{
    nvs_handle_t handle{};
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) return false;
    CredentialsRecord record{};
    std::size_t size = sizeof(record);
    const auto error = nvs_get_blob(handle, key, &record, &size);
    nvs_close(handle);
    if (error != ESP_OK || size != sizeof(record) || record.magic != kCredentialsMagic || record.ssid_length == 0 || record.ssid_length > record.ssid.size() || record.password_length > record.password.size()) {
        wipe_record(record);
        return false;
    }
    ssid.assign(record.ssid.data(), record.ssid_length);
    password.assign(record.password.data(), record.password_length);
    wipe_record(record);
    return true;
}

bool NetworkHttp::save_credentials_to_key(const char* key, const std::string& ssid, const std::string& password) const
{
    CredentialsRecord record{};
    record.magic = kCredentialsMagic;
    record.ssid_length = static_cast<std::uint8_t>(ssid.size());
    record.password_length = static_cast<std::uint8_t>(password.size());
    std::memcpy(record.ssid.data(), ssid.data(), ssid.size());
    std::memcpy(record.password.data(), password.data(), password.size());
    nvs_handle_t handle{};
    const auto open_error = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
    if (open_error != ESP_OK) { wipe_record(record); return false; }
    const auto set_error = nvs_set_blob(handle, key, &record, sizeof(record));
    const auto commit_error = set_error == ESP_OK ? nvs_commit(handle) : set_error;
    nvs_close(handle);
    wipe_record(record);
    return set_error == ESP_OK && commit_error == ESP_OK;
}

bool NetworkHttp::clear_credentials_key(const char* key) const
{
    nvs_handle_t handle{};
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
    auto erase_error = nvs_erase_key(handle, key);
    if (erase_error == ESP_ERR_NVS_NOT_FOUND) erase_error = ESP_OK;
    const auto commit_error = erase_error == ESP_OK ? nvs_commit(handle) : erase_error;
    nvs_close(handle);
    return erase_error == ESP_OK && commit_error == ESP_OK;
}

const char* NetworkHttp::state_name(StaState state) const noexcept
{
    switch (state) {
        case StaState::Idle: return "idle";
        case StaState::Connecting: return "connecting";
        case StaState::Connected: return "connected";
        case StaState::Error: return "error";
    }
    return "error";
}

std::string NetworkHttp::status_json() const
{
    wifi_ap_record_t ap_info{};
    const bool associated = esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
    wifi_config_t sta{};
    std::string ssid;
    if (associated) ssid = ssid_from_bytes(ap_info.ssid, sizeof(ap_info.ssid));
    else if (esp_wifi_get_config(WIFI_IF_STA, &sta) == ESP_OK) ssid = ssid_from_bytes(sta.sta.ssid, sizeof(sta.sta.ssid));
    std::memset(sta.sta.password, 0, sizeof(sta.sta.password));

    std::string ip;
    if (sta_netif_ != nullptr) {
        esp_netif_ip_info_t info{};
        if (esp_netif_get_ip_info(static_cast<esp_netif_t*>(sta_netif_), &info) == ESP_OK && info.ip.addr != 0U) {
            char address[16]{};
            esp_ip4addr_ntoa(&info.ip, address, sizeof(address));
            ip = address;
        }
    }

    auto state = sta_state_.load();
    if (ssid.empty()) state = StaState::Idle;
    if (state == StaState::Connected && ip.empty()) state = StaState::Connecting;
    std::string out = std::string{"{\"ok\":true,\"state\":\""} + state_name(state) + "\",\"ssid\":\"" + json_escape(ssid) + "\",\"ip\":\"" + json_escape(ip) + "\",\"apSsid\":\"" + json_escape(ap_ssid_) + "\"";
    if (associated) out += ",\"rssi\":" + std::to_string(static_cast<int>(ap_info.rssi));
    if (candidate_pending_.load()) out += ",\"candidatePending\":true";
    const int reason = last_disconnect_reason_.load();
    if (reason != 0) out += ",\"lastDisconnectReason\":" + std::to_string(reason);
    out += '}';
    return out;
}

std::string NetworkHttp::scan_json() const
{
    const auto start_error = esp_wifi_scan_start(nullptr, true);
    if (start_error != ESP_OK) return "{\"ok\":false,\"state\":\"error\",\"reason\":\"scan_start_failed\",\"networks\":[]}";
    std::uint16_t count = 0;
    if (esp_wifi_scan_get_ap_num(&count) != ESP_OK) return "{\"ok\":false,\"state\":\"error\",\"reason\":\"scan_count_failed\",\"networks\":[]}";
    count = static_cast<std::uint16_t>(std::min<std::size_t>(count, kMaxScanRecords));
    std::array<wifi_ap_record_t, kMaxScanRecords> records{};
    std::uint16_t returned = count;
    if (returned != 0 && esp_wifi_scan_get_ap_records(&returned, records.data()) != ESP_OK) return "{\"ok\":false,\"state\":\"error\",\"reason\":\"scan_records_failed\",\"networks\":[]}";
    std::string out = "{\"ok\":true,\"networks\":[";
    for (std::uint16_t i = 0; i < returned; ++i) {
        if (i != 0) out += ',';
        const auto ssid = ssid_from_bytes(records[i].ssid, sizeof(records[i].ssid));
        out += "{\"ssid\":\"" + json_escape(ssid) + "\",\"rssi\":" + std::to_string(static_cast<int>(records[i].rssi)) + "}";
    }
    out += "]}";
    return out;
}

}  // namespace homeguard::idf
