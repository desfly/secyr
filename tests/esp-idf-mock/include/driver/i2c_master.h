#pragma once
#include "esp_err.h"
#include "driver/gpio.h"
#include <cstddef>
#include <cstdint>
using i2c_master_bus_handle_t = void*;
using i2c_master_dev_handle_t = void*;
enum { I2C_NUM_0=0, I2C_CLK_SRC_DEFAULT=0, I2C_ADDR_BIT_LEN_7=0 };
struct i2c_master_bus_config_t {
    int i2c_port; gpio_num_t sda_io_num; gpio_num_t scl_io_num; int clk_source;
    int glitch_ignore_cnt; int intr_priority; int trans_queue_depth;
    struct { unsigned enable_internal_pullup:1; unsigned allow_pd:1; } flags;
};
struct i2c_device_config_t {
    int dev_addr_length; std::uint16_t device_address; std::uint32_t scl_speed_hz;
    std::uint32_t scl_wait_us; struct { unsigned disable_ack_check:1; } flags;
};
inline esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t*, i2c_master_bus_handle_t* h){*h=(void*)1;return ESP_OK;}
inline esp_err_t i2c_master_probe(i2c_master_bus_handle_t,std::uint16_t,int){return ESP_OK;}
inline esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t,const i2c_device_config_t*,i2c_master_dev_handle_t* h){*h=(void*)1;return ESP_OK;}
inline esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t){return ESP_OK;}
inline esp_err_t i2c_master_transmit(i2c_master_dev_handle_t,const std::uint8_t*,std::size_t,int){return ESP_OK;}
inline esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t,const std::uint8_t*,std::size_t,std::uint8_t*,std::size_t,int){return ESP_OK;}
