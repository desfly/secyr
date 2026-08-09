#pragma once

#include "esp_err.h"
#include "mqtt_client.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace homeguard::idf {

class CloudLink {
public:
    using CommandHandler = void (*)(const char* payload, std::size_t length, void* context);

    esp_err_t prepare_identity();
    esp_err_t start(const char* broker_uri, const char* username, const char* password);
    void stop();
    void set_command_handler(CommandHandler handler, void* context = nullptr)
    {
        command_handler_ = handler;
        command_context_ = context;
    }

    [[nodiscard]] const char* device_id() const { return device_id_.data(); }
    [[nodiscard]] bool configured() const { return configured_; }
    [[nodiscard]] bool connected() const { return connected_; }
    [[nodiscard]] std::uint32_t connect_count() const { return connect_count_; }
    [[nodiscard]] std::uint32_t disconnect_count() const { return disconnect_count_; }
    [[nodiscard]] std::uint32_t command_count() const { return command_count_; }

    esp_err_t publish_state(const char* json, int qos = 1, bool retain = true);
    esp_err_t publish_command_response(const char* json);

private:
    static void mqtt_event_handler(void* handler_args,
                                   esp_event_base_t base,
                                   std::int32_t event_id,
                                   void* event_data);
    void on_mqtt_event(esp_mqtt_event_handle_t event);
    void make_device_id();
    void make_topics();
    void publish_online(bool online);

    esp_mqtt_client_handle_t client_{};
    std::array<char, 32> device_id_{};
    std::array<char, 96> state_topic_{};
    std::array<char, 96> availability_topic_{};
    std::array<char, 96> command_topic_{};
    std::array<char, 96> response_topic_{};
    CommandHandler command_handler_{};
    void* command_context_{};
    bool configured_{};
    bool connected_{};
    std::uint32_t connect_count_{};
    std::uint32_t disconnect_count_{};
    std::uint32_t command_count_{};
};

}  // namespace homeguard::idf
