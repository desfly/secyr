#include "hg_rs485.hpp"
#include "hg_board_hw678.hpp"
#include "esp_check.h"
#include <cstdint>
#include <cstddef>
#include "freertos/FreeRTOS.h"

#include "driver/uart.h"

namespace homeguard::idf {

std::uint16_t modbus_crc16(
    const std::uint8_t* data,
    std::size_t size) noexcept
{
    std::uint16_t crc = 0xFFFF;

    for (std::size_t index = 0; index < size; ++index) {
        crc ^= data[index];

        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 1U) != 0) {
                crc =
                    static_cast<std::uint16_t>(
                        (crc >> 1) ^ 0xA001U);
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

esp_err_t Rs485Runtime::initialize(
    std::uint32_t baud_rate)
{
    uart_config_t config{};
    config.baud_rate = static_cast<int>(baud_rate);
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.rx_flow_ctrl_thresh = 0;
    config.source_clk = UART_SCLK_DEFAULT;
    config.flags.backup_before_sleep = false;
    config.flags.allow_pd = false;

    ESP_RETURN_ON_ERROR(
        uart_driver_install(
            UART_NUM_1,
            512,
            512,
            0,
            nullptr,
            0),
        "rs485",
        "driver install");

    ESP_RETURN_ON_ERROR(
        uart_param_config(
            UART_NUM_1,
            &config),
        "rs485",
        "parameter config");

    ESP_RETURN_ON_ERROR(
        uart_set_pin(
            UART_NUM_1,
            board::kRs485Tx,
            board::kRs485Rx,
            board::kRs485De,
            UART_PIN_NO_CHANGE),
        "rs485",
        "pin config");

    ESP_RETURN_ON_ERROR(
        uart_set_mode(
            UART_NUM_1,
            UART_MODE_RS485_HALF_DUPLEX),
        "rs485",
        "half-duplex mode");

    initialized_ = true;
    return ESP_OK;
}

esp_err_t Rs485Runtime::transact(
    const std::uint8_t* request,
    std::size_t request_size,
    std::uint8_t* response,
    std::size_t response_capacity,
    std::size_t* response_size,
    std::uint32_t timeout_ms)
{
    if (!initialized_ ||
        request == nullptr ||
        request_size == 0 ||
        response == nullptr ||
        response_size == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    uart_flush_input(UART_NUM_1);

    const int written = uart_write_bytes(
        UART_NUM_1,
        request,
        request_size);
    if (written != static_cast<int>(request_size)) {
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(
        uart_wait_tx_done(
            UART_NUM_1,
            pdMS_TO_TICKS(timeout_ms)),
        "rs485",
        "tx wait");

    const int read = uart_read_bytes(
        UART_NUM_1,
        response,
        response_capacity,
        pdMS_TO_TICKS(timeout_ms));

    if (read <= 0) {
        *response_size = 0;
        return ESP_ERR_TIMEOUT;
    }

    *response_size = static_cast<std::size_t>(read);
    return ESP_OK;
}

bool Rs485Runtime::ready() const noexcept
{
    return initialized_;
}

}  // namespace homeguard::idf
