#pragma once
#include <cstdint>
#include <string>
#include <string_view>

struct esp_mqtt_client;

struct CloudTransportConfig {
    std::string broker_uri;
    std::string device_id;
    std::string access_token;
};

class CloudTransport {
public:
    using CommandHandler = void (*)(std::string_view payload, void* context);
    CloudTransport() = default;
    ~CloudTransport();
    CloudTransport(const CloudTransport&) = delete;
    CloudTransport& operator=(const CloudTransport&) = delete;

    bool begin(CloudTransportConfig config, CommandHandler handler = nullptr, void* context = nullptr);
    void stop();
    bool publish_status(std::string_view json, int qos = 1, bool retain = true);
    bool publish_ack(std::string_view json, int qos = 1);
    [[nodiscard]] bool connected() const { return connected_; }
private:
    static void event_entry(void* handler_args, const char* base, int32_t event_id, void* event_data);
    void handle_event(int32_t event_id, void* event_data);
    CloudTransportConfig config_{};
    std::string command_topic_;
    std::string status_topic_;
    std::string ack_topic_;
    esp_mqtt_client* client_{nullptr};
    CommandHandler command_handler_{nullptr};
    void* command_context_{nullptr};
    bool connected_{false};
};
