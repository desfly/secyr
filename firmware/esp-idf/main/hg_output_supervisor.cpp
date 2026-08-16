#include "hg_output_supervisor.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace homeguard::idf {
namespace {

constexpr const char* kTag = "hg_outputs";
constexpr TickType_t kSupervisorPeriod = pdMS_TO_TICKS(20);

}  // namespace

esp_err_t OutputSupervisor::start(
    hg::PhysicalOutputRuntime* runtime,
    const hg::SystemModel* model)
{
    if (runtime == nullptr || model == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    runtime_ = runtime;
    model_ = model;

    const auto result = xTaskCreate(
        &OutputSupervisor::task_entry,
        "hg_output_guard",
        4096,
        this,
        7,
        nullptr);
    return result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void OutputSupervisor::task_entry(void* context)
{
    static_cast<OutputSupervisor*>(context)->run();
}

void OutputSupervisor::run()
{
    auto last = runtime_->state().status;

    while (true) {
        const auto now_ms = static_cast<std::uint64_t>(esp_timer_get_time() / 1000);
        (void)runtime_->synchronize(*model_, now_ms);

        const auto snapshot = runtime_->state();
        if (snapshot.status != last) {
            if (snapshot.status == hg::PhysicalOutputStatus::Ready) {
                ESP_LOGI(kTag, "Physical output supervisor: %s", hg::to_string(snapshot.status));
            } else {
                ESP_LOGW(kTag,
                         "Physical output supervisor: %s, cold=%s hot=%s, faults=%lu timeouts=%lu",
                         hg::to_string(snapshot.status),
                         hg::to_string(snapshot.cold_valve.direction),
                         hg::to_string(snapshot.hot_valve.direction),
                         static_cast<unsigned long>(snapshot.failures),
                         static_cast<unsigned long>(snapshot.valve_timeouts));
            }
            last = snapshot.status;
        }

        vTaskDelay(kSupervisorPeriod);
    }
}

}  // namespace homeguard::idf
