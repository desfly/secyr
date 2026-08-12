#pragma once

#include "esp_err.h"
#include "mqtt_client.h"

#include <array>
#include <cstdint>

namespace homeguard { class AccessControl; }
namespace hg { class SystemModel; class SystemEventBus; }

namespace homeguard::idf {

class CloudLink {
public:
    esp_err_t prepare_identity();
    esp_err_t start(const char* broker_uri, const char* username, const char* password);
    void stop();
    void set_command_runtime(hg::SystemModel* model, hg::SystemEventBus* bus, homeguard::AccessControl* access_control);

    [[nodiscard]] const char* device_id() const { return device_id_.data(); }
    [[nodiscard]] bool configured() const { return configured_; }
    [[nodiscard]] bool connected() const { return connected_; }
    [[nodiscard]] std::uint32_t connect_count() const { return connect_count_; }
    [[nodiscard]] std::uint32_t disconnect_count() const { return disconnect_count_; }

    esp_err_t publish_state(const char* json, int qos = 1, bool retain = true);

private:
    static void mqtt_event_handler(void* handler_args,
                                   esp_event_base_t base,
                                   std::int32_t event_id,
                                   void* event_data);
    void on_mqtt_event(esp_mqtt_event_handle_t event);
    void make_device_id();
    void make_topics();
    void publish_online(bool online);
    void handle_command(const char* data, std::size_t size);

    esp_mqtt_client_handle_t client_{};
    std::array<char, 32> device_id_{};
    std::array<char, 96> state_topic_{};
    std::array<char, 96> availability_topic_{};
    std::array<char, 96> command_topic_{};
    std::array<char, 96> response_topic_{};
    hg::SystemModel* model_{};
    hg::SystemEventBus* bus_{};
    homeguard::AccessControl* access_control_{};
    bool configured_{};
    bool connected_{};
    std::uint32_t connect_count_{};
    std::uint32_t disconnect_count_{};
};

}  // namespace homeguard::idf
