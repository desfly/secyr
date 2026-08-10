#include "physical_button.hpp"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace { constexpr char tag[] = "hg_button"; }

bool PhysicalButtonService::begin(const int gpio, const bool active_low, const hg::ServiceButtonConfig config,
                                  Callback reboot_callback, Callback factory_reset_callback, void* context) {
    if (running_ || gpio < 0 || gpio > 48 || !reboot_callback || !factory_reset_callback) return false;
    gpio_config_t io{};
    io.pin_bit_mask = 1ULL << static_cast<unsigned>(gpio);
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    io.pull_down_en = active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&io) != ESP_OK) return false;

    gpio_ = gpio;
    active_low_ = active_low;
    button_ = hg::ServiceButton(config);
    reboot_callback_ = reboot_callback;
    reset_callback_ = factory_reset_callback;
    context_ = context;
    running_ = true;
    if (xTaskCreate(&PhysicalButtonService::task_entry, "hg_button", 3072, this, 5, nullptr) != pdPASS) {
        running_ = false;
        return false;
    }
    ESP_LOGI(tag, "physical reset button enabled: short press=reboot, hold=factory reset");
    return true;
}

void PhysicalButtonService::stop() { running_ = false; }

void PhysicalButtonService::task_entry(void* context) {
    static_cast<PhysicalButtonService*>(context)->task();
    vTaskDelete(nullptr);
}

void PhysicalButtonService::task() {
    while (running_) {
        const int level = gpio_get_level(static_cast<gpio_num_t>(gpio_));
        const bool pressed = active_low_ ? level == 0 : level != 0;
        const auto event = button_.update(pressed, static_cast<uint64_t>(esp_timer_get_time() / 1000));
        if (event == hg::ServiceButtonEvent::RebootRequested) {
            reboot_callback_(context_);
        } else if (event == hg::ServiceButtonEvent::FactoryResetRequested) {
            reset_callback_(context_);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
