#pragma once

#include "esp_err.h"
#include "homeguard/system_model.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace homeguard::idf {

class BleTransport {
public:
    using MessageHandler = void(*)(std::uint8_t type, const std::string& payload, void* context);
    using RemoteEventHandler = void(*)(const hg::BleRemoteEvent& event, void* context);

    esp_err_t start(const char* device_name);
    void set_message_handler(MessageHandler handler, void* context);
    void set_remote_event_handler(RemoteEventHandler handler, void* context);

    bool connected() const;
    bool link_connected() const;
    bool notifications_enabled() const { return notify_enabled_; }
    bool runtime_ready() const { return runtime_ready_; }
    std::uint32_t connection_epoch() const { return connection_epoch_; }

    static bool active_runtime_ready();
    static bool active_connection();
    static bool active_notifications();
    static std::uint32_t active_connection_epoch();

    esp_err_t publish_telemetry(const hg::TelemetryFrame& frame);
    esp_err_t send_message(std::uint8_t type, const std::string& payload);

    std::uint8_t* own_address_type_storage();
    void on_connected(std::uint16_t handle);
    void on_disconnected();
    void on_notify_subscription(bool enabled);
    esp_err_t advertise();
    esp_err_t scan_remotes();
    void accept_remote_advertisement(
        std::uint8_t address_type,
        const std::uint8_t address[6],
        const std::uint8_t* payload,
        std::size_t payload_size);
    int accept_rx_fragment(const std::uint8_t* data, std::size_t size);

private:
    esp_err_t notify_message(std::uint8_t type, const std::string& payload);
    void reset_rx();

    MessageHandler message_handler_ = nullptr;
    void* message_context_ = nullptr;
    RemoteEventHandler remote_event_handler_ = nullptr;
    void* remote_event_context_ = nullptr;
    std::uint16_t connection_handle_ = 0xffff;
    bool notify_enabled_ = false;
    bool runtime_ready_ = false;
    std::uint32_t connection_epoch_ = 0;
    std::uint8_t own_address_type_ = 0;
    std::uint16_t next_message_id_ = 1;

    std::uint8_t rx_type_ = 0;
    std::uint16_t rx_message_id_ = 0;
    std::uint8_t rx_expected_count_ = 0;
    std::uint8_t rx_next_index_ = 0;
    std::string rx_payload_;
};

}  // namespace homeguard::idf
