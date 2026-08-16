#pragma once
#include "esp_err.h"
#include "driver/rmt_tx.h"
#include <cstdint>

struct rmt_symbol_word_t { std::uint32_t duration0:15; std::uint32_t level0:1; std::uint32_t duration1:15; std::uint32_t level1:1; };
struct rmt_bytes_encoder_config_t {
    rmt_symbol_word_t bit0{};
    rmt_symbol_word_t bit1{};
    struct { unsigned msb_first:1; } flags{};
};
inline esp_err_t rmt_new_bytes_encoder(const rmt_bytes_encoder_config_t*,rmt_encoder_handle_t* out){ if(out) *out=reinterpret_cast<void*>(1); return ESP_OK; }
inline esp_err_t rmt_del_encoder(rmt_encoder_handle_t){ return ESP_OK; }
