#include "hg_input_runtime.hpp"
#include "hg_input_nvs.hpp"
#include "hg_board_hw678.hpp"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#include <cstdint>

namespace homeguard::idf {

namespace {
constexpr std::uint32_t kPollMs = 50;
constexpr std::uint8_t kStableSamples = 3;

struct DebouncedInput {
    gpio_num_t gpio;
    std::uint16_t source_id;
    InputPolarity polarity;
    bool initialized{};
    bool stable_high{};
    bool candidate_high{};
    std::uint8_t candidate_count{};
};
}

esp_err_t InputRuntime::start(hg::SystemEventBus* bus)
{
    if (bus == nullptr) return ESP_ERR_INVALID_ARG;
    bus_ = bus;

    InputPolarityConfig persisted{};
    const InputNvsStore store;
    const auto load_error = store.load(persisted);
    polarity_ = load_error == ESP_OK ? persisted : InputPolarityConfig{};

    gpio_config_t config{};
    config.pin_bit_mask =
        (1ULL << static_cast<unsigned>(homeguard::board::kTamper)) |
        (1ULL << static_cast<unsigned>(homeguard::board::kPowerFail));
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    const auto gpio_error = gpio_config(&config);
    if (gpio_error != ESP_OK) return gpio_error;

    const auto result = xTaskCreate(&InputRuntime::task_entry, "hg_inputs", 3072, this, 7, nullptr);
    return result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void InputRuntime::task_entry(void* context)
{
    static_cast<InputRuntime*>(context)->run();
}

void InputRuntime::run()
{
    std::array<DebouncedInput, 2> inputs{{
        {homeguard::board::kTamper, 0, polarity_.tamper},
        {homeguard::board::kPowerFail, 1, polarity_.power_fail},
    }};

    while (true) {
        inputs[0].polarity = polarity_.tamper;
        inputs[1].polarity = polarity_.power_fail;

        for (auto& input : inputs) {
            const bool raw_high = gpio_get_level(input.gpio) != 0;
            if (!input.initialized) {
                input.initialized = true;
                input.stable_high = raw_high;
                input.candidate_high = raw_high;
                input.candidate_count = 0;
                continue;
            }

            if (raw_high == input.stable_high) {
                input.candidate_high = raw_high;
                input.candidate_count = 0;
                continue;
            }

            if (raw_high != input.candidate_high) {
                input.candidate_high = raw_high;
                input.candidate_count = 1;
                continue;
            }

            if (input.candidate_count < kStableSamples) ++input.candidate_count;
            if (input.candidate_count < kStableSamples) continue;

            input.stable_high = input.candidate_high;
            input.candidate_count = 0;
            const auto now_ms = static_cast<std::uint64_t>(esp_timer_get_time() / 1000);
            const hg::SystemEvent raw_event{
                .type = hg::SystemEventType::InputChanged,
                .source_id = input.source_id,
                .timestamp_ms = now_ms,
                .sequence = 0,
                .value = input.stable_high ? 1 : 0,
            };
            if (bus_->publish(raw_event)) (void)bus_->dispatch_all();

            if (input.polarity != InputPolarity::Unknown) {
                const bool active = input_is_active(input.polarity, input.stable_high);
                const hg::SystemEvent logical_event{
                    .type = input.source_id == 0
                        ? hg::SystemEventType::Tamper
                        : hg::SystemEventType::PowerFail,
                    .source_id = input.source_id,
                    .timestamp_ms = now_ms,
                    .sequence = 0,
                    .value = active ? 1 : 0,
                };
                if (bus_->publish(logical_event)) (void)bus_->dispatch_all();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(kPollMs));
    }
}

}  // namespace homeguard::idf
