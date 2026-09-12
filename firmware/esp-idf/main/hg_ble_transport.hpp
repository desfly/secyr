#pragma once

#include "homeguard/telemetry.hpp"
#include "esp_err.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace homeguard::idf {

class BleTransport {
public:
    using MessageHandler = void (*)(std::uint8_t type, const std::string& json, void* context);

    esp_err_t start(const char* device_name);
    void set_message_handler(MessageHandler handler, void* context);
    esp_err_t publish_telemetry(const hg::TelemetryFrame& frame);
    esp_err_t send_message(std::uint8_t type, const std::string& payload);

    // A BLE link can exist before the client subscribes to TX notifications.
    // Keep those states separate so diagnostics do not report a live GATT link
    // as disconnected while Android is still completing service setup.
    bool link_connected() const;
    bool notifications_enabled() const { return notify_enabled_; }
    bool connected() const;
    static bool active_connection();
    static bool active_notifications();
    static std::uint32_t active_connection_epoch();
    std::uint32_t connection_epoch() const { return connection_epoch_; }

    esp_err_t advertise();
    int accept_rx_fragment(const std::uint8_t* data, std::size_t size);
    void on_connected(std::uint16_t handle);
    void on_disconnected();
    void on_notify_subscription(bool enabled);
    std::uint8_t* own_address_type_storage();

private:
    esp_err_t notify_message(std::uint8_t type, const std::string& payload);
    void reset_rx();

    std::uint16_t connection_handle_{0xffff};
    std::uint16_t next_message_id_{1};
    std::uint32_t connection_epoch_{0};
    std::uint8_t own_address_type_{0};
    bool notify_enabled_{false};

    std::uint8_t rx_type_{0};
    std::uint16_t rx_message_id_{0};
    std::uint8_t rx_expected_count_{0};
    std::uint8_t rx_next_index_{0};
    std::string rx_payload_{};

    MessageHandler message_handler_{nullptr};
    void* message_context_{nullptr};
};

}  // namespace homeguard::idf
