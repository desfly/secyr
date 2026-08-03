#include "cloud_transport.hpp"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include <utility>

namespace { constexpr char tag[] = "hg_cloud"; }

CloudTransport::~CloudTransport() { stop(); }

bool CloudTransport::begin(CloudTransportConfig config, CommandHandler handler, void* context) {
    if (client_) return true;
    if (config.broker_uri.empty() || config.device_id.empty() || config.access_token.empty()) return false;
    config_ = std::move(config);
    command_handler_ = handler;
    command_context_ = context;
    command_topic_ = "homeguard/" + config_.device_id + "/commands";
    status_topic_ = "homeguard/" + config_.device_id + "/status";
    ack_topic_ = "homeguard/" + config_.device_id + "/acks";

    esp_mqtt_client_config_t mqtt{};
    mqtt.broker.address.uri = config_.broker_uri.c_str();
    mqtt.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
    mqtt.credentials.client_id = config_.device_id.c_str();
    mqtt.credentials.username = config_.device_id.c_str();
    mqtt.credentials.authentication.password = config_.access_token.c_str();
    mqtt.session.keepalive = 30;
    mqtt.session.disable_clean_session = false;
    mqtt.network.reconnect_timeout_ms = 5000;

    client_ = esp_mqtt_client_init(&mqtt);
    if (!client_) return false;
    esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY, &CloudTransport::event_entry, this);
    if (esp_mqtt_client_start(client_) != ESP_OK) {
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
        return false;
    }
    ESP_LOGI(tag, "outbound TLS/MQTT session starting for %s", config_.device_id.c_str());
    return true;
}

void CloudTransport::stop() {
    if (!client_) return;
    esp_mqtt_client_stop(client_);
    esp_mqtt_client_destroy(client_);
    client_ = nullptr;
    connected_ = false;
}

bool CloudTransport::publish_status(std::string_view json, int qos, bool retain) {
    if (!client_ || !connected_) return false;
    return esp_mqtt_client_publish(client_, status_topic_.c_str(), json.data(), static_cast<int>(json.size()), qos, retain ? 1 : 0) >= 0;
}

bool CloudTransport::publish_ack(std::string_view json, int qos) {
    if (!client_ || !connected_) return false;
    return esp_mqtt_client_publish(client_, ack_topic_.c_str(), json.data(), static_cast<int>(json.size()), qos, 0) >= 0;
}

void CloudTransport::event_entry(void* handler_args, const char*, int32_t event_id, void* event_data) {
    static_cast<CloudTransport*>(handler_args)->handle_event(event_id, event_data);
}

void CloudTransport::handle_event(int32_t event_id, void* event_data) {
    auto* event = static_cast<esp_mqtt_event_handle_t>(event_data);
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            connected_ = true;
            esp_mqtt_client_subscribe(client_, command_topic_.c_str(), 1);
            ESP_LOGI(tag, "cloud connected");
            break;
        case MQTT_EVENT_DISCONNECTED:
            connected_ = false;
            ESP_LOGW(tag, "cloud disconnected");
            break;
        case MQTT_EVENT_DATA:
            if (command_handler_ && event->topic_len == static_cast<int>(command_topic_.size()) &&
                std::string_view(event->topic, static_cast<size_t>(event->topic_len)) == command_topic_ &&
                event->current_data_offset == 0 && event->data_len == event->total_data_len) {
                command_handler_(std::string_view(event->data, static_cast<size_t>(event->data_len)), command_context_);
            }
            break;
        default:
            break;
    }
}
