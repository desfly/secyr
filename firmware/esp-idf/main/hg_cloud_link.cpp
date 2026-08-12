#include "hg_cloud_link.hpp"

#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "homeguard/access_control.hpp"
#include "homeguard/system_model.hpp"

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
}

void CloudLink::set_command_runtime(
    hg::SystemModel* model,
    hg::SystemEventBus* bus,
    homeguard::AccessControl* access_control)
{
    model_ = model;
    bus_ = bus;
    access_control_ = access_control;
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
    ESP_LOGI(kTag, "Cloud link started: device=%s broker=%s", device_id_.data(), broker_uri);
    return ESP_OK;
}

void CloudLink::stop()
{
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

esp_err_t CloudLink::publish_state(const char* json, int qos, bool retain)
{
    if (client_ == nullptr || !connected_ || json == nullptr) return ESP_ERR_INVALID_STATE;
    const int id = esp_mqtt_client_publish(client_, state_topic_.data(), json, 0, qos, retain ? 1 : 0);
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
    std::string request_id, actor, credential, command;
    (void)parse_json_string(body, "request_id", request_id);
    if (request_id.empty()) (void)parse_json_string(body, "requestId", request_id);

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
    if (!parse_json_string(body, "command", command) ||
        !parse_json_string(body, "actor", actor) ||
        !parse_json_string(body, "credential", credential)) {
        publish_response(false, "invalid_request");
        return;
    }

    const auto decision = access_control_->authorize(actor, credential, command);
    std::fill(credential.begin(), credential.end(), '\0');
    if (decision != homeguard::AuditDecision::Allowed) {
        publish_response(false, homeguard::to_string(decision));
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
            (void)esp_mqtt_client_subscribe(client_, command_topic_.data(), 1);
            ESP_LOGI(kTag, "Cloud connected; commands=%s responses=%s", command_topic_.data(), response_topic_.data());
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
