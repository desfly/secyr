#pragma once
#include "esp_err.h"
#include "driver/gpio.h"
#include <cstddef>
#include <cstdint>
enum { UART_NUM_1=1, UART_DATA_8_BITS=8, UART_PARITY_DISABLE=0, UART_STOP_BITS_1=1,
UART_HW_FLOWCTRL_DISABLE=0, UART_SCLK_DEFAULT=0, UART_MODE_RS485_HALF_DUPLEX=1,
UART_PIN_NO_CHANGE=-1 };
struct uart_config_t {
    int baud_rate; int data_bits; int parity; int stop_bits; int flow_ctrl;
    int rx_flow_ctrl_thresh; int source_clk;
    struct { unsigned backup_before_sleep:1; unsigned allow_pd:1; } flags;
};
inline esp_err_t uart_driver_install(int,int,int,int,void*,int){return ESP_OK;}
inline esp_err_t uart_param_config(int,const uart_config_t*){return ESP_OK;}
inline esp_err_t uart_set_pin(int,int,int,int,int){return ESP_OK;}
inline esp_err_t uart_set_mode(int,int){return ESP_OK;}
inline esp_err_t uart_flush_input(int){return ESP_OK;}
inline int uart_write_bytes(int,const void*,std::size_t n){return (int)n;}
inline esp_err_t uart_wait_tx_done(int,int){return ESP_OK;}
inline int uart_read_bytes(int,std::uint8_t*,std::size_t,int){return 1;}
