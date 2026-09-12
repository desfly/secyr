#include "hg_ble_remote_runtime.hpp"

#include "homeguard/boot_readiness.hpp"
#include "homeguard/physical_output_runtime.hpp"
#include "homeguard/system_model.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "hg_ble_remote";
constexpr std::uint16_t kPartitionId = 1;
constexpr std::uint16_t kLightOutputId = 4;
constexpr std::uint16_t kLockOutputId = 5;
}

void BleRemoteRuntime::configure(
    hg::SystemModel* model,
    hg::BootReadinessReport* readiness,
    hg::PhysicalOutputRuntime* physical,
    hg::SystemEventBus* bus)
{
    model_ = model;
    readiness_ = readiness;
    physical_ = physical;
    bus_ = bus;

    const auto load_error = store_.load(registry_);
    if (load_error == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(kTag, "No persisted BLE keyfobs");
    } else if (load_error != ESP_OK) {
        registry_.clear();
        ESP_LOGW(kTag, "Persisted BLE keyfobs rejected: %s", esp_err_to_name(load_error));
    } else {
        ESP_LOGI(kTag, "Restored %u BLE keyfob(s)", static_cast<unsigned>(registry_.size()));
    }

    if (!tick_task_started_) {
        tick_task_started_ = xTaskCreate(
            &BleRemoteRuntime::tick_task,
            "hg_ble_remote",
            3072,
            this,
            4,
            nullptr) == pdPASS;
        if (!tick_task_started_) ESP_LOGE(kTag, "Unable to start BLE keyfob timer task");
    }
}

void BleRemoteRuntime::tick_task(void* context)
{
    auto* self = static_cast<BleRemoteRuntime*>(context);
    while (self != nullptr) {
        self->tick(static_cast<std::uint64_t>(esp_timer_get_time() / 1000));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(nullptr);
}

void BleRemoteRuntime::persist_registry()
{
    const auto error = store_.save(registry_);
    if (error != ESP_OK) ESP_LOGE(kTag, "BLE keyfob persistence failed: %s", esp_err_to_name(error));
}

bool BleRemoteRuntime::begin_pairing(std::string_view name, std::uint32_t permissions, std::uint64_t now_ms)
{
    if (name.empty() || permissions == 0U || registry_.size() >= hg::BleRemoteRegistry::capacity) return false;
    std::fill(std::begin(pairing_name_), std::end(pairing_name_), '\0');
    const auto count = std::min(name.size(), sizeof(pairing_name_) - 1U);
    std::memcpy(pairing_name_, name.data(), count);
    pairing_permissions_ = permissions;
    pairing_deadline_ms_ = now_ms + pairing_window_ms;
    ESP_LOGI(kTag, "BLE keyfob pairing window opened for %lu ms", static_cast<unsigned long>(pairing_window_ms));
    return true;
}

void BleRemoteRuntime::cancel_pairing()
{
    pairing_deadline_ms_ = 0;
    pairing_permissions_ = 0;
    std::fill(std::begin(pairing_name_), std::end(pairing_name_), '\0');
}

bool BleRemoteRuntime::pairing_active(std::uint64_t now_ms) const
{
    return pairing_deadline_ms_ != 0U && now_ms <= pairing_deadline_ms_;
}

hg::BleRemoteResult BleRemoteRuntime::ingest(const hg::BleRemoteEvent& event, std::uint64_t now_ms)
{
    bool newly_paired = false;
    if (!registry_.find(event.identity) && pairing_active(now_ms)) {
        newly_paired = registry_.bind(
            event.identity,
            hg::BleRemoteProfile::HomeGuardNative,
            pairing_name_,
            pairing_permissions_,
            true);
        if (newly_paired) {
            ESP_LOGI(kTag, "BLE keyfob paired; address type=%u", static_cast<unsigned>(event.identity.address_type));
        }
        cancel_pairing();
    } else if (pairing_deadline_ms_ != 0U && !pairing_active(now_ms)) {
        cancel_pairing();
    }

    auto result = registry_.accept(event);
    if (result.decision != hg::BleRemoteDecision::Accepted) {
        if (newly_paired) persist_registry();
        return result;
    }

    // Persist the monotonic counter before executing the action so a reboot
    // cannot make an already accepted packet valid again.
    persist_registry();
    if (!execute(event.action, now_ms)) {
        return {hg::BleRemoteDecision::InvalidAction, result.binding, result.command};
    }
    publish_remote_event(result, event.action, now_ms);
    return result;
}

void BleRemoteRuntime::tick(std::uint64_t now_ms)
{
    if (pairing_deadline_ms_ != 0U && !pairing_active(now_ms)) cancel_pairing();
    if (lock_deadline_ms_ == 0U || now_ms < lock_deadline_ms_ || model_ == nullptr ||
        readiness_ == nullptr || physical_ == nullptr) return;

    lock_deadline_ms_ = 0;
    if (model_->set_output_active(kLockOutputId, false, now_ms)) {
        (void)physical_->synchronize(*model_, *readiness_);
        if (bus_ != nullptr) (void)bus_->dispatch_all();
        ESP_LOGI(kTag, "BLE keyfob lock pulse completed");
    }
}

bool BleRemoteRuntime::execute(hg::BleRemoteAction action, std::uint64_t now_ms)
{
    if (model_ == nullptr || readiness_ == nullptr || physical_ == nullptr) return false;

    bool changed = false;
    switch (action) {
        case hg::BleRemoteAction::ArmAway:
            changed = model_->set_partition_arm(kPartitionId, hg::PartitionArmState::Away, now_ms);
            break;
        case hg::BleRemoteAction::ArmHome:
            changed = model_->set_partition_arm(kPartitionId, hg::PartitionArmState::Stay, now_ms);
            break;
        case hg::BleRemoteAction::Disarm:
            changed = model_->set_partition_arm(kPartitionId, hg::PartitionArmState::Disarmed, now_ms);
            break;
        case hg::BleRemoteAction::Panic:
            changed = model_->set_partition_arm(kPartitionId, hg::PartitionArmState::Alarm, now_ms);
            break;
        case hg::BleRemoteAction::LightToggle: {
            const auto* light = model_->output(kLightOutputId);
            if (light == nullptr) return false;
            changed = model_->set_output_active(kLightOutputId, !light->active, now_ms);
            break;
        }
        case hg::BleRemoteAction::LockPulse:
            changed = model_->set_output_active(kLockOutputId, true, now_ms);
            if (changed) lock_deadline_ms_ = now_ms + lock_pulse_ms;
            break;
    }

    if (!changed) return false;
    if (!physical_->synchronize(*model_, *readiness_)) {
        if (action == hg::BleRemoteAction::LightToggle || action == hg::BleRemoteAction::LockPulse) {
            const auto output_id = action == hg::BleRemoteAction::LightToggle ? kLightOutputId : kLockOutputId;
            (void)model_->set_output_active(output_id, false, now_ms);
            (void)physical_->force_safe();
            lock_deadline_ms_ = 0;
        }
        return false;
    }
    if (bus_ != nullptr) (void)bus_->dispatch_all();
    return true;
}

void BleRemoteRuntime::publish_remote_event(
    const hg::BleRemoteResult& result,
    hg::BleRemoteAction action,
    std::uint64_t now_ms)
{
    if (bus_ == nullptr || result.binding == nullptr) return;
    (void)bus_->publish({
        hg::SystemEventType::ConfigChanged,
        static_cast<std::uint16_t>(0x7000U + static_cast<std::uint8_t>(action)),
        now_ms,
        0,
        1,
    });
    (void)bus_->dispatch_all();
}

}  // namespace homeguard::idf
