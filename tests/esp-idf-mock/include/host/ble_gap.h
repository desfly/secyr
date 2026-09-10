#pragma once

#include "host/ble_hs.h"

#include <cstdint>

constexpr std::uint8_t BLE_HS_ADV_F_DISC_GEN = 0x02U;
constexpr std::uint8_t BLE_HS_ADV_F_BREDR_UNSUP = 0x04U;
constexpr std::uint8_t BLE_GAP_CONN_MODE_UND = 1U;
constexpr std::uint8_t BLE_GAP_DISC_MODE_GEN = 2U;

constexpr std::uint8_t BLE_GAP_EVENT_CONNECT = 1U;
constexpr std::uint8_t BLE_GAP_EVENT_DISCONNECT = 2U;
constexpr std::uint8_t BLE_GAP_EVENT_ADV_COMPLETE = 3U;
constexpr std::uint8_t BLE_GAP_EVENT_SUBSCRIBE = 4U;

struct ble_gap_conn_desc {};

struct ble_gap_event {
    std::uint8_t type{};
    struct {
        int status{};
        std::uint16_t conn_handle{};
    } connect{};
    struct {
        int reason{};
    } disconnect{};
    struct {
        std::uint16_t attr_handle{};
        std::uint8_t cur_notify{};
    } subscribe{};
};

using ble_gap_event_fn = int (*)(ble_gap_event* event, void* argument);

struct ble_hs_adv_fields {
    std::uint8_t flags{};
    const std::uint8_t* name{};
    std::uint8_t name_len{};
    std::uint8_t name_is_complete{};
    ble_uuid128_t* uuids128{};
    std::uint8_t num_uuids128{};
    std::uint8_t uuids128_is_complete{};
};

struct ble_gap_adv_params {
    std::uint8_t conn_mode{};
    std::uint8_t disc_mode{};
};

inline int ble_gap_adv_set_fields(const ble_hs_adv_fields*) { return 0; }
inline int ble_gap_adv_start(std::uint8_t, const void*, std::int32_t,
                             const ble_gap_adv_params*, ble_gap_event_fn, void*) { return 0; }
