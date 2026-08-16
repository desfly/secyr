#include "hg_access_nvs.hpp"
#include "homeguard/access_store.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"

#include <cstddef>

namespace homeguard::idf {
namespace {
constexpr const char* nvs_namespace = "hg_access";
constexpr const char* nvs_key = "users_v1";
constexpr const char* log_tag = "homeguard_access";
}

esp_err_t AccessNvsStore::load(AccessControl& access) const {
    nvs_handle_t handle{};
    const auto open_error = nvs_open(nvs_namespace, NVS_READONLY, &handle);
    if (open_error == ESP_ERR_NVS_NOT_FOUND) return open_error;
    if (open_error != ESP_OK) return open_error;

    AccessStoreCodec::Image image{};
    std::size_t size = image.size();
    const auto read_error = nvs_get_blob(handle, nvs_key, image.data(), &size);
    nvs_close(handle);
    if (read_error != ESP_OK) return read_error;
    if (size != image.size()) return ESP_ERR_INVALID_SIZE;
    return AccessStoreCodec::decode(image, access) ? ESP_OK : ESP_ERR_INVALID_CRC;
}

esp_err_t AccessNvsStore::save(const AccessControl& access) const {
    const auto started_us = esp_timer_get_time();
    (void)started_us;  // ESP_LOG* is a no-op in host mocks; keep -Werror clean there.
    ESP_LOGI(log_tag, "Access NVS save begin users=%u", static_cast<unsigned>(access.user_count()));

    nvs_handle_t handle{};
    auto error = nvs_open(nvs_namespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) {
        ESP_LOGE(log_tag, "Access NVS open failed err=%s elapsed=%lld ms",
                 esp_err_to_name(error),
                 static_cast<long long>((esp_timer_get_time() - started_us) / 1000));
        return error;
    }

    const auto image = AccessStoreCodec::encode(access);
    ESP_LOGI(log_tag, "Access NVS encode done elapsed=%lld ms",
             static_cast<long long>((esp_timer_get_time() - started_us) / 1000));

    error = nvs_set_blob(handle, nvs_key, image.data(), image.size());
    ESP_LOGI(log_tag, "Access NVS set_blob done err=%s elapsed=%lld ms",
             esp_err_to_name(error),
             static_cast<long long>((esp_timer_get_time() - started_us) / 1000));

    if (error == ESP_OK) {
        error = nvs_commit(handle);
        ESP_LOGI(log_tag, "Access NVS commit done err=%s elapsed=%lld ms",
                 esp_err_to_name(error),
                 static_cast<long long>((esp_timer_get_time() - started_us) / 1000));
    }
    nvs_close(handle);
    return error;
}

esp_err_t AccessNvsStore::erase() const {
    nvs_handle_t handle{};
    auto error = nvs_open(nvs_namespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_erase_key(handle, nvs_key);
    if (error == ESP_ERR_NVS_NOT_FOUND) error = ESP_OK;
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

}  // namespace homeguard::idf
