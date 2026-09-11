#pragma once

#include "host/ble_hs.h"

#include <cstdint>
#include <cstdlib>

constexpr std::uint8_t BLE_GATT_SVC_TYPE_PRIMARY = 1U;
constexpr std::uint16_t BLE_GATT_CHR_F_WRITE = 0x0008U;
constexpr std::uint16_t BLE_GATT_CHR_F_WRITE_NO_RSP = 0x0004U;
constexpr std::uint16_t BLE_GATT_CHR_F_NOTIFY = 0x0010U;
constexpr std::uint16_t BLE_GATT_CHR_F_WRITE_ENC = 0x0100U;
constexpr std::uint16_t BLE_GATT_CHR_F_READ_ENC = 0x0200U;

constexpr std::uint8_t BLE_GATT_ACCESS_OP_WRITE_CHR = 2U;
constexpr int BLE_ATT_ERR_UNLIKELY = 0x0e;
constexpr int BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN = 0x0d;

struct ble_gatt_access_ctxt {
    std::uint8_t op{};
    os_mbuf* om{};
};

using ble_gatt_access_fn = int (*)(std::uint16_t connection_handle,
                                   std::uint16_t attribute_handle,
                                   ble_gatt_access_ctxt* context,
                                   void* argument);

struct ble_gatt_chr_def {
    const ble_uuid_t* uuid{};
    ble_gatt_access_fn access_cb{};
    void* arg{};
    const void* descriptors{};
    std::uint16_t flags{};
    std::uint8_t min_key_size{};
    std::uint16_t* val_handle{};
};

struct ble_gatt_svc_def {
    std::uint8_t type{};
    const ble_uuid_t* uuid{};
    const ble_gatt_svc_def* includes{};
    const ble_gatt_chr_def* characteristics{};
};

inline int ble_gatts_count_cfg(const ble_gatt_svc_def*) { return 0; }
inline int ble_gatts_add_svcs(const ble_gatt_svc_def*) { return 0; }

inline int ble_gatts_notify_custom(std::uint16_t, std::uint16_t, os_mbuf* value)
{
    if (value != nullptr) {
        std::free(value->data);
        std::free(value);
    }
    return 0;
}
