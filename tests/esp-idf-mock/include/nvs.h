#pragma once

#include "esp_err.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using nvs_handle_t = std::uint32_t;

enum nvs_open_mode_t {
    NVS_READONLY = 0,
    NVS_READWRITE = 1,
};

namespace mock_nvs {

struct Value {
    std::vector<std::uint8_t> bytes;
};

struct Namespace {
    std::unordered_map<std::string, Value> values;
};

inline std::unordered_map<std::string, Namespace> namespaces;
inline std::unordered_map<nvs_handle_t, std::string> handles;
inline nvs_handle_t next_handle = 1U;

inline void reset() {
    namespaces.clear();
    handles.clear();
    next_handle = 1U;
}

inline bool has_namespace(const std::string& name) {
    return namespaces.find(name) != namespaces.end();
}

inline bool has_key(const std::string& name, const std::string& key) {
    const auto ns = namespaces.find(name);
    return ns != namespaces.end() && ns->second.values.find(key) != ns->second.values.end();
}

inline void put_blob(const std::string& name, const std::string& key, const void* data, std::size_t size) {
    auto& bytes = namespaces[name].values[key].bytes;
    bytes.assign(size, 0U);
    if (size != 0U && data != nullptr) std::memcpy(bytes.data(), data, size);
}

inline void put_string(const std::string& name, const std::string& key, const std::string& value) {
    put_blob(name, key, value.c_str(), value.size() + 1U);
}

inline void put_u8(const std::string& name, const std::string& key, std::uint8_t value) {
    put_blob(name, key, &value, sizeof(value));
}

inline const std::string* namespace_for_handle(nvs_handle_t handle) {
    const auto found = handles.find(handle);
    return found == handles.end() ? nullptr : &found->second;
}

inline Value* value_for(nvs_handle_t handle, const char* key) {
    const auto* name = namespace_for_handle(handle);
    if (name == nullptr || key == nullptr) return nullptr;
    auto ns = namespaces.find(*name);
    if (ns == namespaces.end()) return nullptr;
    auto value = ns->second.values.find(key);
    return value == ns->second.values.end() ? nullptr : &value->second;
}

inline const Value* value_for(nvs_handle_t handle, const char* key, int) {
    return value_for(handle, key);
}

}  // namespace mock_nvs

inline esp_err_t nvs_open(const char* name, nvs_open_mode_t mode, nvs_handle_t* handle) {
    if (name == nullptr || handle == nullptr) return ESP_ERR_INVALID_ARG;
    auto found = mock_nvs::namespaces.find(name);
    if (found == mock_nvs::namespaces.end()) {
        if (mode == NVS_READONLY) return ESP_ERR_NVS_NOT_FOUND;
        found = mock_nvs::namespaces.emplace(name, mock_nvs::Namespace{}).first;
    }
    const nvs_handle_t created = mock_nvs::next_handle++;
    mock_nvs::handles[created] = found->first;
    *handle = created;
    return ESP_OK;
}

inline void nvs_close(nvs_handle_t handle) {
    mock_nvs::handles.erase(handle);
}

inline esp_err_t nvs_get_blob(nvs_handle_t handle, const char* key, void* data, std::size_t* size) {
    if (size == nullptr) return ESP_ERR_INVALID_ARG;
    const auto* value = mock_nvs::value_for(handle, key, 0);
    if (value == nullptr) return ESP_ERR_NVS_NOT_FOUND;
    const std::size_t required = value->bytes.size();
    if (data == nullptr) {
        *size = required;
        return ESP_OK;
    }
    if (*size < required) {
        *size = required;
        return ESP_ERR_INVALID_SIZE;
    }
    if (required != 0U) std::memcpy(data, value->bytes.data(), required);
    *size = required;
    return ESP_OK;
}

inline esp_err_t nvs_set_blob(nvs_handle_t handle, const char* key, const void* data, std::size_t size) {
    const auto* name = mock_nvs::namespace_for_handle(handle);
    if (name == nullptr || key == nullptr || (size != 0U && data == nullptr)) return ESP_ERR_INVALID_ARG;
    mock_nvs::put_blob(*name, key, data, size);
    return ESP_OK;
}

inline esp_err_t nvs_get_str(nvs_handle_t handle, const char* key, char* data, std::size_t* size) {
    if (size == nullptr) return ESP_ERR_INVALID_ARG;
    const auto* value = mock_nvs::value_for(handle, key, 0);
    if (value == nullptr) return ESP_ERR_NVS_NOT_FOUND;
    const std::size_t required = value->bytes.size();
    if (data == nullptr) {
        *size = required;
        return ESP_OK;
    }
    if (*size < required) {
        *size = required;
        return ESP_ERR_INVALID_SIZE;
    }
    if (required != 0U) std::memcpy(data, value->bytes.data(), required);
    *size = required;
    return ESP_OK;
}

inline esp_err_t nvs_set_str(nvs_handle_t handle, const char* key, const char* value) {
    if (value == nullptr) return ESP_ERR_INVALID_ARG;
    return nvs_set_blob(handle, key, value, std::strlen(value) + 1U);
}

inline esp_err_t nvs_get_u8(nvs_handle_t handle, const char* key, std::uint8_t* value) {
    if (value == nullptr) return ESP_ERR_INVALID_ARG;
    std::size_t size = sizeof(*value);
    return nvs_get_blob(handle, key, value, &size);
}

inline esp_err_t nvs_set_u8(nvs_handle_t handle, const char* key, std::uint8_t value) {
    return nvs_set_blob(handle, key, &value, sizeof(value));
}

inline esp_err_t nvs_erase_key(nvs_handle_t handle, const char* key) {
    const auto* name = mock_nvs::namespace_for_handle(handle);
    if (name == nullptr || key == nullptr) return ESP_ERR_INVALID_ARG;
    auto ns = mock_nvs::namespaces.find(*name);
    if (ns == mock_nvs::namespaces.end()) return ESP_ERR_NVS_NOT_FOUND;
    return ns->second.values.erase(key) == 0U ? ESP_ERR_NVS_NOT_FOUND : ESP_OK;
}

inline esp_err_t nvs_erase_all(nvs_handle_t handle) {
    const auto* name = mock_nvs::namespace_for_handle(handle);
    if (name == nullptr) return ESP_ERR_INVALID_ARG;
    auto ns = mock_nvs::namespaces.find(*name);
    if (ns == mock_nvs::namespaces.end()) return ESP_ERR_NVS_NOT_FOUND;
    ns->second.values.clear();
    return ESP_OK;
}

inline esp_err_t nvs_commit(nvs_handle_t handle) {
    return mock_nvs::namespace_for_handle(handle) == nullptr ? ESP_ERR_INVALID_ARG : ESP_OK;
}
