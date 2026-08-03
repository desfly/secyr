#include "hg_onewire_runtime.hpp"
#include "hg_board_hw678.hpp"
#include <cstdint>
#include <cstddef>

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace homeguard::idf {

namespace {

constexpr std::uint8_t kSearchRom = 0xF0;
constexpr std::uint8_t kSkipRom = 0xCC;
constexpr std::uint8_t kMatchRom = 0x55;
constexpr std::uint8_t kConvertTemperature = 0x44;
constexpr std::uint8_t kReadScratchpad = 0xBE;
constexpr std::uint8_t kDs18b20Family = 0x28;

}  // namespace

esp_err_t OneWireRuntime::initialize()
{
    const gpio_config_t config{
        .pin_bit_mask = 1ULL << board::kOneWire,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    const auto error = gpio_config(&config);
    if (error == ESP_OK) {
        gpio_set_level(board::kOneWire, 1);
        initialized_ = true;
    }
    return error;
}

bool OneWireRuntime::reset()
{
    gpio_set_direction(board::kOneWire, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(board::kOneWire, 0);
    esp_rom_delay_us(480);

    gpio_set_level(board::kOneWire, 1);
    gpio_set_direction(board::kOneWire, GPIO_MODE_INPUT_OUTPUT_OD);
    esp_rom_delay_us(70);

    const bool present =
        gpio_get_level(board::kOneWire) == 0;
    esp_rom_delay_us(410);
    return present;
}

void OneWireRuntime::write_bit(bool value)
{
    gpio_set_direction(board::kOneWire, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(board::kOneWire, 0);

    if (value) {
        esp_rom_delay_us(6);
        gpio_set_level(board::kOneWire, 1);
        esp_rom_delay_us(64);
    } else {
        esp_rom_delay_us(60);
        gpio_set_level(board::kOneWire, 1);
        esp_rom_delay_us(10);
    }
}

bool OneWireRuntime::read_bit()
{
    gpio_set_direction(board::kOneWire, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(board::kOneWire, 0);
    esp_rom_delay_us(6);

    gpio_set_level(board::kOneWire, 1);
    gpio_set_direction(board::kOneWire, GPIO_MODE_INPUT_OUTPUT_OD);
    esp_rom_delay_us(9);

    const bool value =
        gpio_get_level(board::kOneWire) != 0;
    esp_rom_delay_us(55);
    return value;
}

void OneWireRuntime::write_byte(std::uint8_t value)
{
    for (int bit = 0; bit < 8; ++bit) {
        write_bit(((value >> bit) & 1U) != 0);
    }
}

std::uint8_t OneWireRuntime::read_byte()
{
    std::uint8_t value = 0;
    for (int bit = 0; bit < 8; ++bit) {
        if (read_bit()) {
            value |= static_cast<std::uint8_t>(1U << bit);
        }
    }
    return value;
}

std::uint8_t OneWireRuntime::crc8(
    const std::uint8_t* data,
    std::size_t size) noexcept
{
    std::uint8_t crc = 0;
    for (std::size_t index = 0; index < size; ++index) {
        std::uint8_t current = data[index];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint8_t mix =
                static_cast<std::uint8_t>(
                    (crc ^ current) & 0x01U);
            crc >>= 1;
            if (mix != 0) {
                crc ^= 0x8C;
            }
            current >>= 1;
        }
    }
    return crc;
}

bool OneWireRuntime::search_next(
    std::array<std::uint8_t, 8>* rom)
{
    if (rom == nullptr || last_device_ || !reset()) {
        return false;
    }

    write_byte(kSearchRom);

    int bit_number = 1;
    int last_zero = 0;
    int byte_number = 0;
    std::uint8_t byte_mask = 1;

    while (byte_number < 8) {
        const bool id_bit = read_bit();
        const bool complement_bit = read_bit();

        if (id_bit && complement_bit) {
            return false;
        }

        bool direction = false;
        if (id_bit != complement_bit) {
            direction = id_bit;
        } else if (bit_number < last_discrepancy_) {
            direction =
                (search_rom_[byte_number] & byte_mask) != 0;
        } else {
            direction = bit_number == last_discrepancy_;
        }

        if (!direction) {
            last_zero = bit_number;
        }

        if (direction) {
            search_rom_[byte_number] |= byte_mask;
        } else {
            search_rom_[byte_number] &=
                static_cast<std::uint8_t>(~byte_mask);
        }

        write_bit(direction);

        ++bit_number;
        byte_mask <<= 1;
        if (byte_mask == 0) {
            ++byte_number;
            byte_mask = 1;
        }
    }

    last_discrepancy_ = last_zero;
    if (last_discrepancy_ == 0) {
        last_device_ = true;
    }

    if (crc8(search_rom_.data(), 7) != search_rom_[7]) {
        return false;
    }

    *rom = search_rom_;
    return true;
}

esp_err_t OneWireRuntime::discover()
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    count_ = 0;
    last_discrepancy_ = 0;
    last_device_ = false;
    search_rom_.fill(0);

    while (count_ < devices_.size()) {
        std::array<std::uint8_t, 8> rom{};
        if (!search_next(&rom)) {
            break;
        }

        if (rom[0] == kDs18b20Family) {
            devices_[count_].rom = rom;
            devices_[count_].valid = false;
            ++count_;
        }
    }

    return count_ > 0 ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t OneWireRuntime::convert_all()
{
    if (!initialized_ || !reset()) {
        return ESP_ERR_NOT_FOUND;
    }

    write_byte(kSkipRom);
    write_byte(kConvertTemperature);
    vTaskDelay(pdMS_TO_TICKS(800));
    return ESP_OK;
}

esp_err_t OneWireRuntime::read_all()
{
    if (count_ == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    bool any_valid = false;

    for (std::size_t index = 0; index < count_; ++index) {
        auto& device = devices_[index];
        device.valid = false;

        if (!reset()) {
            continue;
        }

        write_byte(kMatchRom);
        for (const auto byte : device.rom) {
            write_byte(byte);
        }
        write_byte(kReadScratchpad);

        std::array<std::uint8_t, 9> scratchpad{};
        for (auto& byte : scratchpad) {
            byte = read_byte();
        }

        if (crc8(scratchpad.data(), 8) != scratchpad[8]) {
            continue;
        }

        const auto raw = static_cast<std::int16_t>(
            static_cast<std::uint16_t>(scratchpad[0]) |
            (static_cast<std::uint16_t>(scratchpad[1]) << 8));
        const float temperature =
            static_cast<float>(raw) / 16.0F;

        if (temperature == 85.0F ||
            temperature < -55.0F ||
            temperature > 125.0F) {
            continue;
        }

        device.temperature_c = temperature;
        device.valid = true;
        any_valid = true;
    }

    return any_valid ? ESP_OK : ESP_FAIL;
}

std::size_t OneWireRuntime::device_count() const noexcept
{
    return count_;
}

const Ds18b20Device* OneWireRuntime::devices() const noexcept
{
    return devices_.data();
}

bool OneWireRuntime::ready() const noexcept
{
    return initialized_;
}

}  // namespace homeguard::idf
