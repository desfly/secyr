#pragma once
#include "esp_err.h"
#include "driver/gpio.h"
#include <cstddef>
#include <cstdint>

using rmt_channel_handle_t = void*;
using rmt_encoder_handle_t = void*;
enum rmt_clock_source_t { RMT_CLK_SRC_DEFAULT = 0 };
struct rmt_tx_channel_config_t {
    gpio_num_t gpio_num{};
    rmt_clock_source_t clk_src{};
    std::uint32_t resolution_hz{};
    std::size_t mem_block_symbols{};
    std::size_t trans_queue_depth{};
};
struct rmt_transmit_config_t {
    int loop_count{};
    struct {
        std::uint32_t eot_level : 1;
        std::uint32_t queue_nonblocking : 1;
    } flags{};
};
inline esp_err_t rmt_new_tx_channel(const rmt_tx_channel_config_t*, rmt_channel_handle_t* out){ if(out) *out=reinterpret_cast<void*>(1); return ESP_OK; }
inline esp_err_t rmt_enable(rmt_channel_handle_t){ return ESP_OK; }
inline esp_err_t rmt_disable(rmt_channel_handle_t){ return ESP_OK; }
inline esp_err_t rmt_del_channel(rmt_channel_handle_t){ return ESP_OK; }
inline esp_err_t rmt_transmit(rmt_channel_handle_t,rmt_encoder_handle_t,const void*,std::size_t,const rmt_transmit_config_t*){ return ESP_OK; }
inline esp_err_t rmt_tx_wait_all_done(rmt_channel_handle_t,int){ return ESP_OK; }
