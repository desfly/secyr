#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

struct os_mbuf {
    std::uint16_t om_len{0};
    std::uint8_t data[512]{};
};

#define OS_MBUF_PKTLEN(om) ((om) ? (om)->om_len : 0)

struct ble_uuid_t {
    std::uint8_t type{0};
};

struct ble_uuid128_t {
    ble_uuid_t u{};
    std::uint8_t value[16]{};
};

#define BLE_UUID128_INIT(...) { { 128 }, { __VA_ARGS__ } }

struct ble_gatt_access_ctxt {
    int op{0};
    os_mbuf* om{nullptr};
};

using ble_gatt_access_fn = int (*)(std::uint16_t, std::uint16_t, ble_gatt_access_ctxt*, void*);

struct ble_gatt_chr_def {
    const ble_uuid_t* uuid{nullptr};
    ble_gatt_access_fn access_cb{nullptr};
    std::uint16_t flags{0};
    std::uint16_t* val_handle{nullptr};
};

struct ble_gatt_svc_def {
    std::uint8_t type{0};
    const ble_uuid_t* uuid{nullptr};
    const ble_gatt_chr_def* characteristics{nullptr};
};

struct ble_gap_event {
    int type{0};
    struct { int status{0}; std::uint16_t conn_handle{0}; } connect{};
    struct { int reason{0}; } disconnect{};
    struct { std::uint16_t attr_handle{0}; int cur_notify{0}; } subscribe{};
};

using ble_gap_event_fn = int (*)(ble_gap_event*, void*);

struct ble_hs_adv_fields {
    std::uint8_t flags{0};
    ble_uuid128_t* uuids128{nullptr};
    std::uint8_t num_uuids128{0};
    std::uint8_t uuids128_is_complete{0};
};

struct ble_gap_adv_params {
    std::uint8_t conn_mode{0};
    std::uint8_t disc_mode{0};
};

struct ble_hs_cfg_t {
    void (*sync_cb)(){nullptr};
    std::uint8_t sm_bonding{0};
    std::uint8_t sm_sc{0};
    std::uint8_t sm_mitm{0};
    std::uint8_t sm_io_cap{0};
};

inline ble_hs_cfg_t ble_hs_cfg{};

constexpr int BLE_GATT_ACCESS_OP_WRITE_CHR = 1;
constexpr int BLE_ATT_ERR_UNLIKELY = 0x0e;
constexpr int BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN = 0x0d;
constexpr std::uint16_t BLE_GATT_CHR_F_WRITE = 0x0008;
constexpr std::uint16_t BLE_GATT_CHR_F_WRITE_NO_RSP = 0x0010;
constexpr std::uint16_t BLE_GATT_CHR_F_NOTIFY = 0x0020;
constexpr std::uint16_t BLE_GATT_CHR_F_WRITE_ENC = 0x0100;
constexpr std::uint16_t BLE_GATT_CHR_F_READ_ENC = 0x0200;
constexpr std::uint8_t BLE_GATT_SVC_TYPE_PRIMARY = 1;
constexpr std::uint16_t BLE_HS_CONN_HANDLE_NONE = 0xffff;
constexpr int BLE_GAP_EVENT_CONNECT = 1;
constexpr int BLE_GAP_EVENT_DISCONNECT = 2;
constexpr int BLE_GAP_EVENT_ADV_COMPLETE = 3;
constexpr int BLE_GAP_EVENT_SUBSCRIBE = 4;
constexpr std::uint8_t BLE_HS_ADV_F_DISC_GEN = 0x02;
constexpr std::uint8_t BLE_HS_ADV_F_BREDR_UNSUP = 0x04;
constexpr std::uint8_t BLE_GAP_CONN_MODE_UND = 1;
constexpr std::uint8_t BLE_GAP_DISC_MODE_GEN = 1;
constexpr std::uint8_t BLE_HS_IO_NO_INPUT_OUTPUT = 3;
constexpr std::int32_t BLE_HS_FOREVER = -1;

inline int ble_hs_mbuf_to_flat(os_mbuf* om, void* dst, std::uint16_t max_len, std::uint16_t* out_len) {
    if (!om || !dst || !out_len) return -1;
    const auto n = om->om_len < max_len ? om->om_len : max_len;
    std::memcpy(dst, om->data, n);
    *out_len = n;
    return 0;
}

inline os_mbuf* ble_hs_mbuf_from_flat(const void* src, std::uint16_t len) {
    static os_mbuf buffer{};
    if (len > sizeof(buffer.data)) return nullptr;
    buffer.om_len = len;
    if (src && len) std::memcpy(buffer.data, src, len);
    return &buffer;
}

inline int ble_gatts_count_cfg(const ble_gatt_svc_def*) { return 0; }
inline int ble_gatts_add_svcs(const ble_gatt_svc_def*) { return 0; }
inline int ble_gatts_notify_custom(std::uint16_t, std::uint16_t, os_mbuf*) { return 0; }
inline std::uint16_t ble_att_mtu(std::uint16_t) { return 247; }
inline int ble_hs_id_infer_auto(int, std::uint8_t* out_addr_type) { if (out_addr_type) *out_addr_type = 0; return 0; }
inline int ble_gap_adv_set_fields(const ble_hs_adv_fields*) { return 0; }
inline int ble_gap_adv_start(std::uint8_t, const void*, std::int32_t, const ble_gap_adv_params*, ble_gap_event_fn, void*) { return 0; }

extern "C" inline void ble_store_config_init(void) {}
