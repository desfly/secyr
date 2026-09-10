#pragma once

#include "homeguard/telemetry.hpp"
#include "esp_err.h"

#include <array>
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
    bool connected() const;

private:
    friend int hg_ble_rx_access(std::uint16_t, std::uint16_t, struct ble_gatt_access_ctxt*, void*);
    friend int hg_ble_gap_event(struct ble_gap_event*, void*);
    friend void hg_ble_stack_sync();

    esp_err_t advertise();
    int accept_rx_fragment(const std::uint8_t* data, std::size_t size);
    esp_err_t notify_message(std::uint8_t type, const std::string& payload);
    void reset_rx();

    std::uint16_t connection_handle_{0xffff};
    std::uint16_t next_message_id_{1};
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
