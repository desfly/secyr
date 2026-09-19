#include "hg_cloud_link.hpp"
#include "hg_cloud_command_verifier.hpp"
#include "hg_cloud_trust_nvs.hpp"
#include "hg_cloud_trusted_time.hpp"

#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "homeguard/access_control.hpp"
#include "homeguard/system_model.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "hg_cloud";
constexpr const char* kPrefix = "homeguard/v1/devices";
constexpr std::uint64_t kHeartbeatPeriodUs = 60ULL * 1000ULL * 1000ULL;
constexpr char kCloudSecurityNamespace[] = "hg-cloud-sec";
constexpr char kCommandCounterKey[] = "cmd_counter";
constexpr char kRequestIdKey[] = "req_id";
constexpr std::uint64_t kMaxCommandTtlMs = 120000ULL;
constexpr std::uint64_t kMaxIssuedFutureSkewMs = 30000ULL;
constexpr std::size_t kDisarmChallengeLength = 32U;
constexpr std::uint64_t kDisarmChallengeTtlUs = 60ULL * 1000ULL * 1000ULL;
std::string g_disarm_challenge;
std::uint64_t g_disarm_challenge_deadline_us{};
StaticSemaphore_t g_replay_admission_mutex_storage{};
SemaphoreHandle_t g_replay_admission_mutex = nullptr;

SemaphoreHandle_t replay_admission_mutex()
{
    if (g_replay_admission_mutex == nullptr) {
        g_replay_admission_mutex = xSemaphoreCreateMutexStatic(&g_replay_admission_mutex_storage);
    }
    return g_replay_admission_mutex;
}

std::string issue_disarm_challenge()
{
    char token[33]{};
    std::snprintf(token, sizeof(token), "%08lx%08lx%08lx%08lx",
                  static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()),
                  static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
    g_disarm_challenge.assign(token);
    g_disarm_challenge_deadline_us = static_cast<std::uint64_t>(esp_timer_get_time()) + kDisarmChallengeTtlUs;
    return g_disarm_challenge;
}

bool valid_disarm_challenge(const std::string& challenge)
{
    const auto now = static_cast<std::uint64_t>(esp_timer_get_time());
    if (g_disarm_challenge.size() != kDisarmChallengeLength ||
        challenge.size() != kDisarmChallengeLength ||
        now > g_disarm_challenge_deadline_us) {
        return false;
    }

    unsigned char difference = 0;
    for (std::size_t i = 0; i < kDisarmChallengeLength; ++i) {
        const auto ch = static_cast<unsigned char>(challenge[i]);
        const bool hex = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
        if (!hex) return false;
        difference |= static_cast<unsigned char>(
            ch ^ static_cast<unsigned char>(g_disarm_challenge[i]));
    }
    return difference == 0;
}

void consume_disarm_challenge()
{
    std::fill(g_disarm_challenge.begin(), g_disarm_challenge.end(), '\0');
    g_disarm_challenge.clear();
    g_disarm_challenge_deadline_us = 0;
}

bool parse_json_u64(const std::string& body, const char* key, std::uint64_t& value)
{
    const std::string marker = std::string{"\""} + key + "\"";
    auto pos = body.find(marker);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + marker.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
    if (pos >= body.size() || !std::isdigit(static_cast<unsigned char>(body[pos]))) return false;
    std::uint64_t parsed = 0;
    do {
        const auto digit = static_cast<unsigned>(body[pos] - '0');
        if (parsed > (UINT64_MAX - digit) / 10U) return false;
        parsed = parsed * 10U + digit;
        ++pos;
    } while (pos < body.size() && std::isdigit(static_cast<unsigned char>(body[pos])));
    value = parsed;
    return true;
}

bool load_command_counter(std::uint64_t& value)
{
    nvs_handle_t handle{};
    if (nvs_open(kCloudSecurityNamespace, NVS_READONLY, &handle) != ESP_OK) {
        value = 0;
        return true;
    }
    const auto error = nvs_get_u64(handle, kCommandCounterKey, &value);
    nvs_close(handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) { value = 0; return true; }
    return error == ESP_OK;
}

bool load_last_request_id(std::string& value)
{
    nvs_handle_t handle{};
    if (nvs_open(kCloudSecurityNamespace, NVS_READONLY, &handle) != ESP_OK) {
        value.clear();
        return true;
    }
    std::size_t size = 0;
    auto error = nvs_get_str(handle, kRequestIdKey, nullptr, &size);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        value.clear();
        return true;
    }
    if (error != ESP_OK || size == 0 || size > 129U) {
        nvs_close(handle);
        return false;
    }
    std::string stored(size, '\0');
    error = nvs_get_str(handle, kRequestIdKey, stored.data(), &size);
    nvs_close(handle);
    if (error != ESP_OK) return false;
    if (!stored.empty() && stored.back() == '\0') stored.pop_back();
    value = std::move(stored);
    return true;
}

bool persist_command_replay_state(std::uint64_t counter, const std::string& request_id)
{
    nvs_handle_t handle{};
    if (nvs_open(kCloudSecurityNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
    auto error = nvs_set_u64(handle, kCommandCounterKey, counter);
    if (error == ESP_OK) error = nvs_set_str(handle, kRequestIdKey, request_id.c_str());
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error == ESP_OK;
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

bool canonical_text_safe(const std::string& value)
{
    for (const unsigned char ch : value) {
        if (ch < 0x20U || ch == 0x7fU) return false;
    }
    return true;
}

std::string json_escape(const std::string& value)
{
    std::string out;
    out.reserve(value.size() + 8);
    for (const char ch : value) {
        if (ch == '"') out += "\\\"";
        else if (ch == '\\') out += "\\\\";
        else if (ch == '\n') out += "\\n";
        else if (ch == '\r') out += "\\r";
        else if (ch == '\t') out += "\\t";
        else if (static_cast<unsigned char>(ch) >= 0x20U) out.push_back(ch);
    }
    return out;
}

const char* arm_state_name(hg::PartitionArmState state)
{
    switch (state) {
        case hg::PartitionArmState::Stay: return "stay";
        case hg::PartitionArmState::Away: return "away";
        case hg::PartitionArmState::Alarm: return "alarm";
        default: return "disarmed";
    }
}

const char* event_name(hg::SystemEventType type)
{
    switch (type) {
        case hg::SystemEventType::ZoneOpen: return "zone_open";
        case hg::SystemEventType::ZoneClosed: return "zone_closed";
        case hg::SystemEventType::Alarm: return "alarm";
        case hg::SystemEventType::Tamper: return "tamper";
        case hg::SystemEventType::SensorOffline: return "sensor_offline";
        case hg::SystemEventType::BatteryLow: return "battery_low";
        case hg::SystemEventType::OutputOn: return "output_on";
        case hg::SystemEventType::OutputOff: return "output_off";
        case hg::SystemEventType::Armed: return "armed";
        case hg::SystemEventType::Disarmed: return "disarmed";
        case hg::SystemEventType::ConfigChanged: return "config_changed";
    }
    return "unknown";
}

bool should_publish_event(hg::SystemEventType type)
{
    switch (type) {
        case hg::SystemEventType::ZoneOpen:
        case hg::SystemEventType::ZoneClosed:
        case hg::SystemEventType::Alarm:
        case hg::SystemEventType::Tamper:
        case hg::SystemEventType::SensorOffline:
        case hg::SystemEventType::BatteryLow:
        case hg::SystemEventType::OutputOn:
        case hg::SystemEventType::OutputOff:
        case hg::SystemEventType::Armed:
        case hg::SystemEventType::Disarmed:
            return true;
        case hg::SystemEventType::ConfigChanged:
            return false;
    }
    return false;
}
}

void CloudLink::set_command_runtime(
    hg::SystemModel* model,
    hg::SystemEventBus* bus,
    homeguard::AccessControl* access_control,
    CloudTrustedTime* trusted_time)
{
    model_ = model;
    bus_ = bus;
    access_control_ = access_control;
    trusted_time_ = trusted_time;
    if (bus_ != nullptr && !event_bus_subscribed_) {
        event_bus_subscribed_ = bus_->subscribe(&CloudLink::system_event_handler, this);
        if (!event_bus_subscribed_) ESP_LOGE(kTag, "Cloud event subscription failed");
    }
}

void CloudLink::make_device_id()
{
    std::uint8_t mac[6]{};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        std::strncpy(device_id_.data(), "HG-UNKNOWN", device_id_.size() - 1);
        return;
    }
    std::snprintf(device_id_.data(), device_id_.size(),
                  "HG-%02X%02X%02X%02X%02X%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void CloudLink::make_topics()
{
    std::snprintf(state_topic_.data(), state_topic_.size(), "%s/%s/state", kPrefix, device_id_.data());
    std::snprintf(availability_topic_.data(), availability_topic_.size(), "%s/%s/availability", kPrefix, device_id_.data());
    std::snprintf(heartbeat_topic_.data(), heartbeat_topic_.size(), "%s/%s/heartbeat", kPrefix, device_id_.data());
    std::snprintf(event_topic_.data(), event_topic_.size(), "%s/%s/events", kPrefix, device_id_.data());
    std::snprintf(command_topic_.data(), command_topic_.size(), "%s/%s/commands", kPrefix, device_id_.data());
    std::snprintf(response_topic_.data(), response_topic_.size(), "%s/%s/responses", kPrefix, device_id_.data());
}

esp_err_t CloudLink::prepare_identity()
{
    make_device_id();
    make_topics();
    return device_id_[0] == '\0' ? ESP_FAIL : ESP_OK;
}

esp_err_t CloudLink::start(const char* broker_uri, const char* username, const char* password)
{
    if (client_ != nullptr) return ESP_ERR_INVALID_STATE;
    if (broker_uri == nullptr || broker_uri[0] == '\0') return ESP_ERR_INVALID_ARG;
    // Cloud control is fail-closed: credentials and commands must never be
    // sent over plaintext MQTT, even if a bad broker URI is provisioned.
    if (std::strncmp(broker_uri, "mqtts://", 8) != 0) {
        ESP_LOGE(kTag, "Refusing non-TLS MQTT broker URI");
        return ESP_ERR_INVALID_ARG;
    }
    if (device_id_[0] == '\0') ESP_RETURN_ON_ERROR(prepare_identity(), kTag, "cloud identity");

    const esp_mqtt_client_config_t config = {
        .broker = {
            .address = {.uri = broker_uri},
            .verification = {.crt_bundle_attach = esp_crt_bundle_attach},
        },
        .credentials = {
            .username = username,
            .client_id = device_id_.data(),
            .authentication = {.password = password},
        },
        .session = {
            .last_will = {
                .topic = availability_topic_.data(),
                .msg = "offline",
                .msg_len = 7,
                .qos = 1,
                .retain = 1,
            },
            .keepalive = 30,
        },
        .network = {
            .reconnect_timeout_ms = 5000,
            .timeout_ms = 10000,
        },
    };

    client_ = esp_mqtt_client_init(&config);
    if (client_ == nullptr) return ESP_FAIL;

    auto error = esp_mqtt_client_register_event(
        client_, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
        &CloudLink::mqtt_event_handler, this);
    if (error != ESP_OK) {
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
        return error;
    }

    error = esp_mqtt_client_start(client_);
    if (error != ESP_OK) {
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
        return error;
    }

    configured_ = true;
    ESP_LOGI(kTag, "Cloud link started in low-traffic mode: device=%s broker=%s heartbeat=60s", device_id_.data(), broker_uri);
    return ESP_OK;
}

void CloudLink::stop()
{
    stop_heartbeat_timer();
    if (client_ == nullptr) {
        connected_ = false;
        configured_ = false;
        return;
    }
    if (connected_) publish_online(false);
    (void)esp_mqtt_client_stop(client_);
    esp_mqtt_client_destroy(client_);
    client_ = nullptr;
    connected_ = false;
    configured_ = false;
}

void CloudLink::publish_online(bool online)
{
    if (client_ == nullptr) return;
    const char* value = online ? "online" : "offline";
    (void)esp_mqtt_client_publish(client_, availability_topic_.data(), value, 0, 1, 1);
}

void CloudLink::publish_heartbeat()
{
    if (client_ == nullptr || !connected_) return;
    const auto up_seconds = static_cast<unsigned long long>(esp_timer_get_time() / 1000000LL);
    const auto sequence = static_cast<unsigned long long>(++heartbeat_sequence_);
    char payload[96]{};
    const int length = std::snprintf(payload, sizeof(payload),
        "{\"seq\":%llu,\"up\":%llu,\"online\":true}", sequence, up_seconds);
    if (length <= 0 || static_cast<std::size_t>(length) >= sizeof(payload)) return;
    (void)esp_mqtt_client_publish(client_, heartbeat_topic_.data(), payload, length, 0, 0);
}

void CloudLink::start_heartbeat_timer()
{
    if (heartbeat_timer_ == nullptr) {
        const esp_timer_create_args_t args = {
            .callback = &CloudLink::heartbeat_timer_handler,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "hg_mqtt_hb",
            .skip_unhandled_events = true,
        };
        if (esp_timer_create(&args, &heartbeat_timer_) != ESP_OK) {
            heartbeat_timer_ = nullptr;
            ESP_LOGE(kTag, "MQTT heartbeat timer create failed");
            return;
        }
    }
    if (!esp_timer_is_active(heartbeat_timer_)) {
        if (esp_timer_start_periodic(heartbeat_timer_, kHeartbeatPeriodUs) != ESP_OK) {
            ESP_LOGE(kTag, "MQTT heartbeat timer start failed");
        }
    }
}

void CloudLink::stop_heartbeat_timer()
{
    if (heartbeat_timer_ == nullptr) return;
    if (esp_timer_is_active(heartbeat_timer_)) (void)esp_timer_stop(heartbeat_timer_);
    (void)esp_timer_delete(heartbeat_timer_);
    heartbeat_timer_ = nullptr;
}

void CloudLink::heartbeat_timer_handler(void* context)
{
    auto* self = static_cast<CloudLink*>(context);
    if (self != nullptr) self->publish_heartbeat();
}

void CloudLink::system_event_handler(const hg::SystemEvent& event, void* context)
{
    auto* self = static_cast<CloudLink*>(context);
    if (self != nullptr) self->publish_system_event(event);
}

void CloudLink::publish_system_event(const hg::SystemEvent& event)
{
    if (client_ == nullptr || !connected_ || !should_publish_event(event.type)) return;
    char payload[176]{};
    const int length = std::snprintf(payload, sizeof(payload),
        "{\"event\":\"%s\",\"source\":%u,\"value\":%ld,\"ts\":%llu,\"seq\":%llu}",
        event_name(event.type),
        static_cast<unsigned>(event.source_id),
        static_cast<long>(event.value),
        static_cast<unsigned long long>(event.timestamp_ms),
        static_cast<unsigned long long>(event.sequence));
    if (length <= 0 || static_cast<std::size_t>(length) >= sizeof(payload)) return;
    (void)esp_mqtt_client_publish(client_, event_topic_.data(), payload, length, 1, 0);
}

esp_err_t CloudLink::publish_state(const char* json, int qos, bool retain)
{
    if (client_ == nullptr || !connected_ || json == nullptr) return ESP_ERR_INVALID_STATE;
    const int id = esp_mqtt_client_publish(client_, state_topic_.data(), json, 0, qos, retain ? 1 : 0);
    return id >= 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t CloudLink::issue_disarm_challenge()
{
    if (client_ == nullptr || !connected_) return ESP_ERR_INVALID_STATE;
    const std::string challenge = ::homeguard::idf::issue_disarm_challenge();
    const std::string payload = std::string{"{\"type\":\"security.disarm_challenge\",\"challenge\":\""} +
                                json_escape(challenge) + "\",\"ttlMs\":60000}";
    const int id = esp_mqtt_client_publish(client_, response_topic_.data(), payload.c_str(), 0, 1, 0);
    return id >= 0 ? ESP_OK : ESP_FAIL;
}

void CloudLink::mqtt_event_handler(void* handler_args,
                                   esp_event_base_t,
                                   std::int32_t,
                                   void* event_data)
{
    auto* self = static_cast<CloudLink*>(handler_args);
    if (self == nullptr || event_data == nullptr) return;
    self->on_mqtt_event(static_cast<esp_mqtt_event_handle_t>(event_data));
}

void CloudLink::handle_command(const char* data, std::size_t size)
{
    if (client_ == nullptr || data == nullptr || size == 0) return;
    const std::string body(data, size);
    std::string request_id, actor, command, challenge, signature;
    (void)parse_json_string(body, "requestId", request_id);

    auto publish_response = [&](bool ok, const char* reason, const char* arm_state = nullptr) {
        std::string response = std::string{"{\"ok\":"} + (ok ? "true" : "false") +
            ",\"requestId\":\"" + json_escape(request_id) + "\"";
        if (reason != nullptr) response += ",\"reason\":\"" + json_escape(reason) + "\"";
        if (arm_state != nullptr) response += ",\"armState\":\"" + std::string(arm_state) + "\"";
        response += '}';
        (void)esp_mqtt_client_publish(client_, response_topic_.data(), response.c_str(), 0, 1, 0);
    };

    if (model_ == nullptr || bus_ == nullptr || access_control_ == nullptr) {
        publish_response(false, "runtime_unavailable");
        return;
    }
    std::uint64_t version = 0, key_epoch = 0, command_counter = 0, issued_at_ms = 0, expires_at_ms = 0;
    std::string envelope_device_id;
    if (!parse_json_u64(body, "version", version) || version != 1 ||
        !parse_json_string(body, "deviceId", envelope_device_id) ||
        envelope_device_id != device_id_.data() ||
        !parse_json_string(body, "command", command) ||
        !parse_json_string(body, "actor", actor) ||
        !parse_json_u64(body, "keyEpoch", key_epoch) || key_epoch == 0 ||
        !parse_json_u64(body, "counter", command_counter) || command_counter == 0 ||
        !parse_json_u64(body, "issuedAtMs", issued_at_ms) ||
        !parse_json_u64(body, "expiresAtMs", expires_at_ms) ||
        !parse_json_string(body, "challenge", challenge) ||
        !parse_json_string(body, "signature", signature)) {
        publish_response(false, "invalid_request");
        return;
    }

    // The signature transcript is line-oriented (key=value\\n). Reject control
    // characters before canonicalization so signed fields cannot inject lines or
    // create an ambiguous transcript.
    if (!canonical_text_safe(envelope_device_id) || !canonical_text_safe(request_id) ||
        !canonical_text_safe(actor) || !canonical_text_safe(command) ||
        !canonical_text_safe(challenge)) {
        publish_response(false, "invalid_canonical_text");
        return;
    }

    if (request_id.empty() || request_id.size() > 128U || expires_at_ms <= issued_at_ms ||
        (expires_at_ms - issued_at_ms) > kMaxCommandTtlMs) {
        publish_response(false, "invalid_freshness_window");
        return;
    }
    if (trusted_time_ == nullptr || !trusted_time_->ready()) {
        publish_response(false, "trusted_time_unavailable");
        return;
    }
    const auto now_ms = trusted_time_->now_ms();
    if (now_ms == 0 || now_ms > expires_at_ms ||
        issued_at_ms > now_ms + kMaxIssuedFutureSkewMs) {
        publish_response(false, "stale_or_future_command");
        return;
    }

    CloudCommandTrust trust;
    CloudTrustStore trust_store;
    if (trust_store.load(trust) != ESP_OK || trust.version == 0U || trust.public_key_pem.empty()) {
        publish_response(false, "command_trust_unavailable");
        return;
    }
    // Bind every signed command to the currently active trust/key generation.
    // Protocol version and key epoch are intentionally independent: rotating
    // credentials must invalidate old signed envelopes without changing v1.
    if (key_epoch != trust.version) {
        publish_response(false, "key_epoch_rejected");
        return;
    }
    const std::string canonical =
        "version=" + std::to_string(version) + "\n" +
        "deviceId=" + envelope_device_id + "\n" +
        "requestId=" + request_id + "\n" +
        "actor=" + actor + "\n" +
        "command=" + command + "\n" +
        "keyEpoch=" + std::to_string(key_epoch) + "\n" +
        "counter=" + std::to_string(command_counter) + "\n" +
        "issuedAtMs=" + std::to_string(issued_at_ms) + "\n" +
        "expiresAtMs=" + std::to_string(expires_at_ms) + "\n" +
        "challenge=" + challenge;
    CloudCommandVerifier verifier;
    if (verifier.verify(trust.public_key_pem, canonical, signature) != ESP_OK) {
        publish_response(false, "signature_rejected");
        return;
    }

    // Remote disarm is a high-risk action: require a signed, non-empty challenge.
    // One-time replay resistance is jointly provided by the persisted monotonic
    // counter/requestId state below; a replayed signed envelope cannot execute.
    if (command == "security.disarm" &&
        !valid_disarm_challenge(challenge)) {
        publish_response(false, "challenge_required");
        return;
    }

    // The signed envelope authenticates the cloud command and binds actor.
    // Challenge issuance is part of the disarm flow, so it inherits exactly the
    // actor permission required for security.disarm instead of introducing a
    // separate role capability. Never transport a user PIN over MQTT.
    const std::string_view authorization_command =
        command == "security.disarm_challenge" ? std::string_view{"security.disarm"}
                                                : std::string_view{command};
    const auto decision = access_control_->authorize_session(actor, authorization_command);
    if (decision != homeguard::AuditDecision::Allowed) {
        publish_response(false, homeguard::to_string(decision));
        return;
    }

    // Serialize replay admission across MQTT callbacks/tasks. The lock covers
    // read -> validate -> durable NVS commit, so two concurrent envelopes cannot
    // both pass against the same previously stored counter.
    const auto replay_mutex = replay_admission_mutex();
    if (replay_mutex == nullptr || xSemaphoreTake(replay_mutex, portMAX_DELAY) != pdTRUE) {
        publish_response(false, "replay_state_unavailable");
        return;
    }

    // A trust rotation can happen while the signature is being verified.
    // Re-read the active trust after acquiring replay admission and fail closed
    // if either the epoch or the key changed before the durable replay commit.
    // This is a defense-in-depth check, not a substitute for serializing the
    // trust writer with command execution during transactional rotation.
    CloudCommandTrust admission_trust;
    if (trust_store.load(admission_trust) != ESP_OK ||
        admission_trust.version != trust.version ||
        admission_trust.public_key_pem != trust.public_key_pem) {
        xSemaphoreGive(replay_mutex);
        publish_response(false, "key_epoch_changed");
        return;
    }

    std::uint64_t stored_counter = 0;
    std::string last_request_id;
    const bool replay_state_loaded =
        load_command_counter(stored_counter) && load_last_request_id(last_request_id);
    if (!replay_state_loaded) {
        xSemaphoreGive(replay_mutex);
        publish_response(false, "replay_state_unavailable");
        return;
    }
    if (command_counter <= stored_counter) {
        xSemaphoreGive(replay_mutex);
        publish_response(false, "replay_rejected");
        return;
    }
    if (request_id == last_request_id) {
        xSemaphoreGive(replay_mutex);
        publish_response(false, "request_replay_rejected");
        return;
    }

    // Challenge issuance is itself a signed, authorized and replay-protected
    // operation. Reuse the same persisted counter/requestId barrier as commands.
    if (command == "security.disarm_challenge") {
        if (!persist_command_replay_state(command_counter, request_id)) {
            xSemaphoreGive(replay_mutex);
            publish_response(false, "replay_state_persist_failed");
            return;
        }
        xSemaphoreGive(replay_mutex);
        if (issue_disarm_challenge() != ESP_OK) {
            publish_response(false, "challenge_issue_failed");
            return;
        }
        publish_response(true, "challenge_issued");
        return;
    }

    hg::PartitionArmState target{};
    if (command == "security.arm_away") target = hg::PartitionArmState::Away;
    else if (command == "security.arm_home") target = hg::PartitionArmState::Stay;
    else if (command == "security.disarm") target = hg::PartitionArmState::Disarmed;
    else if (command == "security.panic") target = hg::PartitionArmState::Alarm;
    else {
        publish_response(false, "unsupported_command");
        return;
    }

    // Persist the monotonic counter before consuming a one-time challenge or
    // applying the side effect. An NVS failure must not burn a valid challenge,
    // while a reboot must not reopen the replay window.
    if (!persist_command_replay_state(command_counter, request_id)) {
        xSemaphoreGive(replay_mutex);
        publish_response(false, "replay_state_persist_failed");
        return;
    }
    xSemaphoreGive(replay_mutex);

    // Signature, authorization, replay checks and durable replay-state update
    // have all succeeded. Only now burn the one-time disarm token.
    if (command == "security.disarm") consume_disarm_challenge();

    if (!model_->set_partition_arm(1, target, 0)) {
        publish_response(false, "partition_command_failed");
        return;
    }
    (void)bus_->dispatch_all();
    publish_response(true, "accepted", arm_state_name(target));
}

void CloudLink::on_mqtt_event(esp_mqtt_event_handle_t event)
{
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            connected_ = true;
            ++connect_count_;
            publish_online(true);
            publish_heartbeat();
            start_heartbeat_timer();
            (void)esp_mqtt_client_subscribe(client_, command_topic_.data(), 1);
            ESP_LOGI(kTag, "Cloud connected; low-traffic heartbeat=60s events=%s", event_topic_.data());
            break;
        case MQTT_EVENT_DISCONNECTED:
            connected_ = false;
            ++disconnect_count_;
            ESP_LOGW(kTag, "Cloud disconnected");
            break;
        case MQTT_EVENT_DATA: {
            const std::string topic(event->topic, static_cast<std::size_t>(event->topic_len));
            if (topic == command_topic_.data()) {
                handle_command(event->data, static_cast<std::size_t>(event->data_len));
            }
            break;
        }
        case MQTT_EVENT_ERROR:
            ESP_LOGE(kTag, "Cloud MQTT error");
            break;
        default:
            break;
    }
}

}  // namespace homeguard::idf
