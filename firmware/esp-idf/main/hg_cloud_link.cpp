#include "hg_cloud_link.hpp"

#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_mac.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "hg_cloud";
constexpr const char* kPrefix = "homeguard/v1/devices";

bool is_ping_command(const std::string& data)
{
    return data == "ping" || data.find("\"type\":\"ping\"") != std::string::npos ||
           data.find("\"command\":\"ping\"") != std::string::npos;
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
    if (client_ == nullptr) return;
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

void CloudLink::publish_command_response(const char* json)
{
    if (client_ == nullptr || !connected_ || json == nullptr) return;
    (void)esp_mqtt_client_publish(client_, response_topic_.data(), json, 0, 1, 0);
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

void CloudLink::on_mqtt_event(esp_mqtt_event_handle_t event)
{
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            connected_ = true;
            ++connect_count_;
            publish_online(true);
            (void)esp_mqtt_client_subscribe(client_, command_topic_.data(), 1);
            publish_command_response("{\"event\":\"cloud_connected\",\"ok\":true}");
            ESP_LOGI(kTag, "Cloud connected; commands=%s responses=%s",
                     command_topic_.data(), response_topic_.data());
            break;
        case MQTT_EVENT_DISCONNECTED:
            connected_ = false;
            ++disconnect_count_;
            ESP_LOGW(kTag, "Cloud disconnected");
            break;
        case MQTT_EVENT_DATA: {
            const std::string topic(event->topic, static_cast<std::size_t>(event->topic_len));
            const std::string data(event->data, static_cast<std::size_t>(event->data_len));
            if (topic == command_topic_.data()) {
                ++command_count_;
                if (is_ping_command(data)) {
                    char response[192]{};
                    std::snprintf(response, sizeof(response),
                                  "{\"ok\":true,\"type\":\"pong\",\"device_id\":\"%s\",\"command_count\":%lu}",
                                  device_id_.data(), static_cast<unsigned long>(command_count_));
                    publish_command_response(response);
                    ESP_LOGI(kTag, "Cloud ping handled");
                } else {
                    publish_command_response("{\"ok\":false,\"error\":\"unsupported_command\"}");
                    ESP_LOGW(kTag, "Unsupported cloud command received (%u bytes)",
                             static_cast<unsigned>(data.size()));
                }
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
