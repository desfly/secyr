#include "hg_pzem004t.hpp"

#include "freertos/FreeRTOS.h"
#include "driver/uart.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace homeguard::idf {

namespace {
constexpr std::size_t kReadResponseSize = 25;
constexpr std::size_t kReadRegisterCount = 10;

std::uint16_t be16(const std::uint8_t* data) noexcept
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[0]) << 8U) |
        static_cast<std::uint16_t>(data[1]));
}

std::uint32_t low_high_u32(std::uint16_t low, std::uint16_t high) noexcept
{
    return static_cast<std::uint32_t>(low) |
           (static_cast<std::uint32_t>(high) << 16U);
}
}  // namespace

std::uint16_t Pzem004t::crc16(const std::uint8_t* data, std::size_t size) noexcept
{
    std::uint16_t crc = 0xFFFFU;
    for (std::size_t index = 0; index < size; ++index) {
        crc ^= data[index];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x0001U) != 0U
                ? static_cast<std::uint16_t>((crc >> 1U) ^ 0xA001U)
                : static_cast<std::uint16_t>(crc >> 1U);
        }
    }
    return crc;
}

esp_err_t Pzem004t::initialize(
    uart_port_t uart_port,
    int tx_gpio,
    int rx_gpio,
    std::uint8_t address)
{
    if (uart_port < UART_NUM_0 || uart_port >= UART_NUM_MAX ||
        tx_gpio < 0 || rx_gpio < 0 || tx_gpio == rx_gpio ||
        address == 0x00 || address > 0xF8) {
        return ESP_ERR_INVALID_ARG;
    }

    uart_config_t config{};
    config.baud_rate = 9600;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.rx_flow_ctrl_thresh = 0;
    config.source_clk = UART_SCLK_DEFAULT;
    config.flags.backup_before_sleep = false;
    config.flags.allow_pd = false;

    auto error = uart_driver_install(uart_port, 256, 256, 0, nullptr, 0);
    if (error != ESP_OK) return error;

    error = uart_param_config(uart_port, &config);
    if (error != ESP_OK) {
        uart_driver_delete(uart_port);
        return error;
    }

    error = uart_set_pin(
        uart_port,
        tx_gpio,
        rx_gpio,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE);
    if (error != ESP_OK) {
        uart_driver_delete(uart_port);
        return error;
    }

    uart_port_ = uart_port;
    address_ = address;
    initialized_ = true;
    return ESP_OK;
}

esp_err_t Pzem004t::transact(
    const std::uint8_t* request,
    std::size_t request_size,
    std::uint8_t* response,
    std::size_t response_capacity,
    std::size_t* response_size,
    std::uint32_t timeout_ms)
{
    if (!initialized_ || request == nullptr || request_size == 0 ||
        response == nullptr || response_size == nullptr ||
        response_capacity == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    *response_size = 0;
    uart_flush_input(uart_port_);

    const int written = uart_write_bytes(uart_port_, request, request_size);
    if (written != static_cast<int>(request_size)) return ESP_FAIL;

    auto error = uart_wait_tx_done(uart_port_, pdMS_TO_TICKS(timeout_ms));
    if (error != ESP_OK) return error;

    const int read = uart_read_bytes(
        uart_port_, response, response_capacity, pdMS_TO_TICKS(timeout_ms));
    if (read <= 0) return ESP_ERR_TIMEOUT;

    *response_size = static_cast<std::size_t>(read);
    return ESP_OK;
}

esp_err_t Pzem004t::read(Pzem004tReading* reading, std::uint32_t timeout_ms)
{
    if (!initialized_ || reading == nullptr) return ESP_ERR_INVALID_ARG;

    std::array<std::uint8_t, 8> request{
        address_, 0x04, 0x00, 0x00, 0x00,
        static_cast<std::uint8_t>(kReadRegisterCount), 0x00, 0x00};
    const auto request_crc = crc16(request.data(), 6);
    request[6] = static_cast<std::uint8_t>(request_crc & 0xFFU);
    request[7] = static_cast<std::uint8_t>((request_crc >> 8U) & 0xFFU);

    std::array<std::uint8_t, kReadResponseSize> response{};
    std::size_t response_size = 0;
    auto error = transact(
        request.data(), request.size(), response.data(), response.size(),
        &response_size, timeout_ms);
    if (error != ESP_OK) return error;

    if (response_size >= 5 && response[0] == address_ &&
        response[1] == static_cast<std::uint8_t>(0x04U | 0x80U)) {
        return ESP_FAIL;
    }

    if (response_size != kReadResponseSize ||
        response[0] != address_ || response[1] != 0x04 || response[2] != 20) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const auto response_crc = crc16(response.data(), response_size - 2);
    if (response[response_size - 2] != static_cast<std::uint8_t>(response_crc & 0xFFU) ||
        response[response_size - 1] != static_cast<std::uint8_t>((response_crc >> 8U) & 0xFFU)) {
        return ESP_ERR_INVALID_CRC;
    }

    std::array<std::uint16_t, kReadRegisterCount> reg{};
    for (std::size_t i = 0; i < reg.size(); ++i) {
        reg[i] = be16(&response[3 + i * 2]);
    }

    const auto current_raw = low_high_u32(reg[1], reg[2]);
    const auto power_raw = low_high_u32(reg[3], reg[4]);
    const auto energy_raw = low_high_u32(reg[5], reg[6]);

    reading->voltage_v = static_cast<float>(reg[0]) * 0.1F;
    reading->current_a = static_cast<float>(current_raw) * 0.001F;
    reading->active_power_w = static_cast<float>(power_raw) * 0.1F;
    reading->energy_kwh = static_cast<float>(energy_raw) / 1000.0F;
    reading->frequency_hz = static_cast<float>(reg[7]) * 0.1F;
    reading->power_factor = static_cast<float>(reg[8]) * 0.01F;
    reading->power_alarm = reg[9] == 0xFFFFU;
    return ESP_OK;
}

esp_err_t Pzem004t::reset_energy(std::uint32_t timeout_ms)
{
    if (!initialized_) return ESP_ERR_INVALID_STATE;

    std::array<std::uint8_t, 4> request{address_, 0x42, 0x00, 0x00};
    const auto request_crc = crc16(request.data(), 2);
    request[2] = static_cast<std::uint8_t>(request_crc & 0xFFU);
    request[3] = static_cast<std::uint8_t>((request_crc >> 8U) & 0xFFU);

    std::array<std::uint8_t, 4> response{};
    std::size_t response_size = 0;
    auto error = transact(
        request.data(), request.size(), response.data(), response.size(),
        &response_size, timeout_ms);
    if (error != ESP_OK) return error;

    if (response_size != response.size() || response[0] != address_ || response[1] != 0x42) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const auto response_crc = crc16(response.data(), 2);
    if (response[2] != static_cast<std::uint8_t>(response_crc & 0xFFU) ||
        response[3] != static_cast<std::uint8_t>((response_crc >> 8U) & 0xFFU)) {
        return ESP_ERR_INVALID_CRC;
    }
    return ESP_OK;
}

bool Pzem004t::ready() const noexcept
{
    return initialized_;
}

std::uint8_t Pzem004t::address() const noexcept
{
    return address_;
}

}  // namespace homeguard::idf
