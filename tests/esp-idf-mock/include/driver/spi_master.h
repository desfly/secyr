#pragma once
#include "esp_err.h"
#include <cstdint>
enum { SPI2_HOST=2, SPI3_HOST=3, SPI_DMA_CH_AUTO=0, SPI_CLK_SRC_DEFAULT=0 };
struct spi_bus_config_t {
    int mosi_io_num,miso_io_num,sclk_io_num,quadwp_io_num,quadhd_io_num;
    int data4_io_num,data5_io_num,data6_io_num,data7_io_num,max_transfer_sz,flags,intr_flags;
};
struct spi_device_interface_config_t {
    int command_bits,address_bits,dummy_bits,mode,clock_source,duty_cycle_pos;
    int cs_ena_pretrans,cs_ena_posttrans,clock_speed_hz,input_delay_ns,spics_io_num;
    int flags,queue_size; void* pre_cb; void* post_cb;
};
inline esp_err_t spi_bus_initialize(int,const spi_bus_config_t*,int){return ESP_OK;}
inline esp_err_t spi_bus_free(int){return ESP_OK;}
