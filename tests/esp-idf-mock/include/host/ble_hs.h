#pragma once

#include "host/ble_uuid.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

struct os_mbuf {
    std::uint16_t len{};
    std::uint8_t* data{};
};

#define OS_MBUF_PKTLEN(om) ((om)->len)

constexpr std::uint16_t BLE_HS_CONN_HANDLE_NONE = 0xffffU;
constexpr std::int32_t BLE_HS_FOREVER = -1;
constexpr std::uint8_t BLE_HS_IO_NO_INPUT_OUTPUT = 3U;

struct ble_hs_cfg_t {
    void (*sync_cb)(){};
    std::uint8_t sm_bonding{};
    std::uint8_t sm_sc{};
    std::uint8_t sm_mitm{};
    std::uint8_t sm_io_cap{};
};

inline ble_hs_cfg_t ble_hs_cfg{};

inline int ble_hs_id_infer_auto(int, std::uint8_t* address_type)
{
    if (address_type != nullptr) *address_type = 0;
    return 0;
}

inline int ble_hs_mbuf_to_flat(const os_mbuf* source, void* destination, std::uint16_t max_len, std::uint16_t* copied)
{
    if (source == nullptr || destination == nullptr || source->len > max_len) return -1;
    if (source->len != 0U && source->data != nullptr) std::memcpy(destination, source->data, source->len);
    if (copied != nullptr) *copied = source->len;
    return 0;
}

inline os_mbuf* ble_hs_mbuf_from_flat(const void* source, std::uint16_t length)
{
    auto* mbuf = static_cast<os_mbuf*>(std::calloc(1, sizeof(os_mbuf)));
    if (mbuf == nullptr) return nullptr;
    mbuf->len = length;
    if (length != 0U) {
        mbuf->data = static_cast<std::uint8_t*>(std::malloc(length));
        if (mbuf->data == nullptr) {
            std::free(mbuf);
            return nullptr;
        }
        if (source != nullptr) std::memcpy(mbuf->data, source, length);
    }
    return mbuf;
}

inline std::uint16_t ble_att_mtu(std::uint16_t)
{
    return 247U;
}
