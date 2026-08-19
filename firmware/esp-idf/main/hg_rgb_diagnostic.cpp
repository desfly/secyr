#include "hg_rgb_diagnostic.hpp"

#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#include <cstdint>

namespace homeguard::idf {

namespace {
constexpr std::uint32_t kResolutionHz = 10'000'000U;  // 0.1 us ticks
constexpr std::uint32_t kT0hTicks = 3U;
constexpr std::uint32_t kT0lTicks = 9U;
constexpr std::uint32_t kT1hTicks = 9U;
constexpr std::uint32_t kT1lTicks = 3U;
constexpr std::uint32_t kWs2812ResetUs = 80U;

esp_err_t transmit_rgb(int gpio, const std::array<std::uint8_t, 3>& grb) {
    rmt_channel_handle_t channel = nullptr;
    rmt_encoder_handle_t encoder = nullptr;

    rmt_tx_channel_config_t channel_config{};
    channel_config.gpio_num = static_cast<gpio_num_t>(gpio);
    channel_config.clk_src = RMT_CLK_SRC_DEFAULT;
    channel_config.resolution_hz = kResolutionHz;
    channel_config.mem_block_symbols = 64;
    channel_config.trans_queue_depth = 1;

    auto error = rmt_new_tx_channel(&channel_config, &channel);
    if (error != ESP_OK) return error;

    rmt_bytes_encoder_config_t encoder_config{};
    encoder_config.bit0.level0 = 1;
    encoder_config.bit0.duration0 = kT0hTicks;
    encoder_config.bit0.level1 = 0;
    encoder_config.bit0.duration1 = kT0lTicks;
    encoder_config.bit1.level0 = 1;
    encoder_config.bit1.duration0 = kT1hTicks;
    encoder_config.bit1.level1 = 0;
    encoder_config.bit1.duration1 = kT1lTicks;
    encoder_config.flags.msb_first = 1;

    error = rmt_new_bytes_encoder(&encoder_config, &encoder);
    if (error == ESP_OK) error = rmt_enable(channel);

    if (error == ESP_OK) {
        rmt_transmit_config_t tx_config{};
        tx_config.loop_count = 0;
        tx_config.eot_level = 0;
        error = rmt_transmit(channel, encoder, grb.data(), grb.size(), &tx_config);
        if (error == ESP_OK) error = rmt_tx_wait_all_done(channel, pdMS_TO_TICKS(100));
        if (error == ESP_OK) esp_rom_delay_us(kWs2812ResetUs);
    }

    if (channel != nullptr) {
        (void)rmt_disable(channel);
    }
    if (encoder != nullptr) {
        (void)rmt_del_encoder(encoder);
    }
    if (channel != nullptr) {
        (void)rmt_del_channel(channel);
    }
    return error;
}
}

bool RgbDiagnostic::supported_gpio(int gpio) {
    return gpio == 38 || gpio == 48;
}

esp_err_t RgbDiagnostic::set_white(int gpio) {
    if (!supported_gpio(gpio)) return ESP_ERR_INVALID_ARG;
    const std::array<std::uint8_t, 3> white{{0xffU, 0xffU, 0xffU}};
    return transmit_rgb(gpio, white);
}

esp_err_t RgbDiagnostic::set_red(int gpio) {
    if (!supported_gpio(gpio)) return ESP_ERR_INVALID_ARG;

    // WS2812 wire order is GRB, therefore red is G=0, R=255, B=0.
    const std::array<std::uint8_t, 3> red{{0x00U, 0xffU, 0x00U}};
    return transmit_rgb(gpio, red);
}

esp_err_t RgbDiagnostic::off(int gpio) {
    if (!supported_gpio(gpio)) return ESP_ERR_INVALID_ARG;
    const std::array<std::uint8_t, 3> off{{0x00U, 0x00U, 0x00U}};
    return transmit_rgb(gpio, off);
}

esp_err_t RgbDiagnostic::test_white(int gpio, unsigned duration_ms) {
    if (!supported_gpio(gpio) || duration_ms == 0U || duration_ms > 5000U) {
        return ESP_ERR_INVALID_ARG;
    }

    auto error = set_white(gpio);
    if (error != ESP_OK) return error;

    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    return off(gpio);
}

}  // namespace homeguard::idf
