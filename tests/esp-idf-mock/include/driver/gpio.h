#pragma once
#include "esp_err.h"
#include <cstdint>
enum gpio_num_t {
GPIO_NUM_0=0, GPIO_NUM_3=3, GPIO_NUM_4=4, GPIO_NUM_5=5, GPIO_NUM_6=6,
GPIO_NUM_7=7, GPIO_NUM_8=8, GPIO_NUM_9=9, GPIO_NUM_10=10, GPIO_NUM_11=11,
GPIO_NUM_12=12, GPIO_NUM_13=13, GPIO_NUM_14=14, GPIO_NUM_15=15,
GPIO_NUM_16=16, GPIO_NUM_17=17, GPIO_NUM_18=18, GPIO_NUM_19=19,
GPIO_NUM_20=20, GPIO_NUM_21=21, GPIO_NUM_35=35, GPIO_NUM_36=36,
GPIO_NUM_37=37, GPIO_NUM_39=39, GPIO_NUM_40=40, GPIO_NUM_41=41,
GPIO_NUM_42=42, GPIO_NUM_43=43, GPIO_NUM_44=44, GPIO_NUM_45=45,
GPIO_NUM_46=46, GPIO_NUM_48=48
};
enum { GPIO_MODE_OUTPUT=0, GPIO_MODE_OUTPUT_OD=1, GPIO_MODE_INPUT_OUTPUT_OD=2, GPIO_MODE_INPUT=3 };
enum { GPIO_PULLUP_DISABLE=0, GPIO_PULLUP_ENABLE=1, GPIO_PULLDOWN_DISABLE=0, GPIO_PULLDOWN_ENABLE=1 };
enum { GPIO_INTR_DISABLE=0 };
struct gpio_config_t {
    std::uint64_t pin_bit_mask;
    int mode;
    int pull_up_en;
    int pull_down_en;
    int intr_type;
};
inline esp_err_t gpio_config(const gpio_config_t*) { return ESP_OK; }
inline esp_err_t gpio_set_level(gpio_num_t,int){return ESP_OK;}
inline esp_err_t gpio_set_direction(gpio_num_t,int){return ESP_OK;}
inline int gpio_get_level(gpio_num_t){return 1;}
