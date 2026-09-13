#include "hg_ble_transport.hpp"
#include "hg_ble_runtime_status.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_port.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

extern "C" void ble_store_config_init(void);

namespace {
constexpr const char* kTag = "hg_ble";
constexpr std::uint8_t kProtocolVersion = 1;
constexpr std::size_t kHeaderSize = 6;
constexpr std::size_t kMaxMessageBytes = 4096;
constexpr std::size_t kMaxGattValue = 244;
constexpr std::uint8_t kTelemetryType = 1;
constexpr std::array<std::uint8_t, 4> kRemoteMagic{{'H','G','K','F'}};
constexpr std::size_t kRemotePayloadBytes = 10;

homeguard::idf::BleTransport* g_owner = nullptr;
std::uint16_t g_tx_value_handle = 0;
TaskHandle_t g_adv_restart_task = nullptr;

void advertising_restart_task(void*) {
    constexpr int kMaxAttempts = 10;
    for (int attempt = 1; attempt <= kMaxAttempts; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(attempt == 1 ? 150 : 500));
        auto* owner = g_owner;
        if (owner == nullptr || owner->link_connected()) break;
        if (ble_gap_adv_active() != 0) {
            homeguard::idf::ble_runtime_status::set_advertising(true);
            ESP_LOGI(kTag,"BLE advertising already active during recovery; attempt=%d",attempt);
            break;
        }
        const auto error = owner->advertise();
        if (error == ESP_OK) {
            ESP_LOGI(kTag,"BLE advertising recovered; attempt=%d",attempt);
            break;
        }
        ESP_LOGW(kTag,"BLE advertising recovery deferred; attempt=%d/%d",attempt,kMaxAttempts);
    }
    g_adv_restart_task = nullptr;
    vTaskDelete(nullptr);
}

void schedule_advertising_restart(const char* reason) {
    if (g_owner == nullptr || g_owner->link_connected()) return;
    if (ble_gap_adv_active() != 0) {
        homeguard::idf::ble_runtime_status::set_advertising(true);
        return;
    }
    if (g_adv_restart_task != nullptr) return;
    ESP_LOGI(kTag,"Scheduling BLE advertising recovery; reason=%s",reason != nullptr ? reason : "unknown");
    if (xTaskCreate(advertising_restart_task,"hg_ble_adv",3072,nullptr,4,&g_adv_restart_task) != pdPASS) {
        g_adv_restart_task = nullptr;
        ESP_LOGE(kTag,"Cannot create BLE advertising recovery task");
    }
}

const ble_uuid128_t kServiceUuid = BLE_UUID128_INIT(
    0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,0x01,0x00,0x40,0x6e);
const ble_uuid128_t kRxUuid = BLE_UUID128_INIT(
    0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,0x02,0x00,0x40,0x6e);
const ble_uuid128_t kTxUuid = BLE_UUID128_INIT(
    0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,0x03,0x00,0x40,0x6e);

int rx_access(std::uint16_t, std::uint16_t, ble_gatt_access_ctxt* ctxt, void*) {
    if (!g_owner || !ctxt || ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;
    const auto length = static_cast<std::size_t>(OS_MBUF_PKTLEN(ctxt->om));
    if (length < kHeaderSize || length > kMaxGattValue) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    std::array<std::uint8_t,kMaxGattValue> value{};
    std::uint16_t copied = 0;
    if (ble_hs_mbuf_to_flat(ctxt->om,value.data(),value.size(),&copied) != 0) return BLE_ATT_ERR_UNLIKELY;
    return g_owner->accept_rx_fragment(value.data(),copied);
}

int tx_access(std::uint16_t, std::uint16_t, ble_gatt_access_ctxt* ctxt, void*) {
    if (!ctxt || ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_UNLIKELY;
    // TX is notification-driven. A zero-length encrypted read keeps the
    // characteristic valid for NimBLE without duplicating telemetry state.
    return 0;
}

const ble_gatt_chr_def kCharacteristics[] = {
    {
        .uuid = &kRxUuid.u,
        .access_cb = rx_access,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC,
    },
    {
        .uuid = &kTxUuid.u,
        .access_cb = tx_access,
        .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ_ENC,
        .val_handle = &g_tx_value_handle,
    },
    {0}
};

const ble_gatt_svc_def kServices[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &kServiceUuid.u,
        .characteristics = kCharacteristics,
    },
    {0}
};

int gap_event(ble_gap_event* event, void*) {
    if (!g_owner || !event) return 0;
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                homeguard::idf::ble_runtime_status::set_advertising(false);
                g_owner->on_connected(event->connect.conn_handle);
            } else {
                homeguard::idf::ble_runtime_status::set_advertising(false);
                schedule_advertising_restart("connect-failed");
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(kTag,"BLE link disconnected; reason=%d",event->disconnect.reason);
            homeguard::idf::ble_runtime_status::set_advertising(false);
            g_owner->on_disconnected();
            schedule_advertising_restart("disconnect");
            break;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            homeguard::idf::ble_runtime_status::set_advertising(false);
            schedule_advertising_restart("adv-complete");
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == g_tx_value_handle) g_owner->on_notify_subscription(event->subscribe.cur_notify != 0);
            break;
        case BLE_GAP_EVENT_ENC_CHANGE:
            ESP_LOGI(
                kTag,
                "BLE encryption change; handle=%u status=%d",
                static_cast<unsigned>(event->enc_change.conn_handle),
                event->enc_change.status);
            break;
        case BLE_GAP_EVENT_DISC: {
            ble_hs_adv_fields fields{};
            if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) == 0 &&
                fields.mfg_data != nullptr && fields.mfg_data_len > 0) {
                g_owner->accept_remote_advertisement(
                    event->disc.addr.type,
                    event->disc.addr.val,
                    fields.mfg_data,
                    fields.mfg_data_len);
            }
            break;
        }
        case BLE_GAP_EVENT_DISC_COMPLETE:
            (void)g_owner->scan_remotes();
            break;
        default: break;
    }
    return 0;
}

void stack_sync() {
    if (!g_owner) return;
    if (ble_hs_id_infer_auto(0,g_owner->own_address_type_storage()) != 0) {
        homeguard::idf::ble_runtime_status::set_advertising(false);
        ESP_LOGE(kTag,"Cannot infer BLE identity address");
        return;
    }
    const auto error = g_owner->advertise();
    if (error != ESP_OK) {
        ESP_LOGE(kTag,"BLE advertising failed: %s",esp_err_to_name(error));
        schedule_advertising_restart("stack-sync");
    }
    const auto scan_error = g_owner->scan_remotes();
    if (scan_error != ESP_OK) ESP_LOGW(kTag,"BLE remote scan start deferred: %s",esp_err_to_name(scan_error));
}

void host_task(void*) {
    nimble_port_run();
    vTaskDelete(nullptr);
}
}

namespace homeguard::idf {
esp_err_t BleTransport::start(const char* device_name) {
    if (!device_name || !device_name[0]) return ESP_ERR_INVALID_ARG;
    if (g_owner && g_owner != this) return ESP_ERR_INVALID_STATE;
    g_owner = this;
    ble_runtime_status::set_advertising(false);
    auto error = nimble_port_init();
    if (error != ESP_OK) {
        ESP_LOGE(kTag,"NimBLE init failed: %s",esp_err_to_name(error));
        return error;
    }
    ble_svc_gap_init();
    ble_svc_gatt_init();
    const int name_rc = ble_svc_gap_device_name_set(device_name);
    if (name_rc != 0) {
        ESP_LOGE(kTag,"BLE GAP device-name setup failed: rc=%d",name_rc);
        return ESP_FAIL;
    }
    int rc = ble_gatts_count_cfg(kServices);
    if (rc != 0) {
        ESP_LOGE(kTag,"BLE GATT service count failed: rc=%d",rc);
        return ESP_FAIL;
    }
    rc = ble_gatts_add_svcs(kServices);
    if (rc != 0) {
        ESP_LOGE(kTag,"BLE GATT service registration failed: rc=%d",rc);
        return ESP_FAIL;
    }
    ble_hs_cfg.sync_cb = stack_sync;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    // Bonding needs encryption/identity keys to be exchanged and persisted.
    // Without these masks Android can remain in BOND_BONDING indefinitely.
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_store_config_init();
    if (xTaskCreate(host_task,"hg_ble_host",4096,nullptr,5,nullptr) != pdPASS) return ESP_ERR_NO_MEM;
    ESP_LOGI(kTag,"NimBLE HomeGuard transport started");
    return ESP_OK;
}

void BleTransport::set_message_handler(MessageHandler handler, void* context) {
    message_handler_=handler;
    message_context_=context;
}

void BleTransport::set_remote_event_handler(RemoteEventHandler handler, void* context) {
    remote_event_handler_ = handler;
    remote_event_context_ = context;
}

bool BleTransport::link_connected() const {
    return connection_handle_ != BLE_HS_CONN_HANDLE_NONE;
}

bool BleTransport::connected() const {
    return link_connected() && notify_enabled_;
}

bool BleTransport::active_connection() {
    return g_owner != nullptr && g_owner->link_connected();
}

bool BleTransport::active_notifications() {
    return g_owner != nullptr && g_owner->notifications_enabled();
}

std::uint32_t BleTransport::active_connection_epoch() {
    return g_owner != nullptr ? g_owner->connection_epoch() : 0U;
}

esp_err_t BleTransport::publish_telemetry(const hg::TelemetryFrame& frame) {
    return notify_message(kTelemetryType,hg::telemetry_json(frame));
}

esp_err_t BleTransport::send_message(std::uint8_t type, const std::string& payload) {
    return notify_message(type, payload);
}

std::uint8_t* BleTransport::own_address_type_storage() {
    return &own_address_type_;
}

void BleTransport::on_connected(std::uint16_t handle) {
    connection_handle_=handle;
    ++connection_epoch_;
    if (connection_epoch_ == 0U) ++connection_epoch_;
    notify_enabled_=false;
    reset_rx();
    ESP_LOGI(kTag,"Android BLE link connected; handle=%u epoch=%lu",handle,static_cast<unsigned long>(connection_epoch_));
}

void BleTransport::on_disconnected() {
    connection_handle_=BLE_HS_CONN_HANDLE_NONE;
    notify_enabled_=false;
    reset_rx();
}

void BleTransport::on_notify_subscription(bool enabled) {
    notify_enabled_=enabled;
    ESP_LOGI(kTag,"BLE telemetry notifications %s",enabled?"enabled":"disabled");
    if (!enabled || !link_connected()) return;

    // Android has completed service discovery and CCCD subscription. Start SMP
    // from the ESP on this live GATT link; Android only observes the bond state
    // instead of racing us with BluetoothDevice.createBond().
    const int security_rc = ble_gap_security_initiate(connection_handle_);
    if (security_rc == 0) {
        ESP_LOGI(kTag,"BLE security initiated by ESP; handle=%u",connection_handle_);
    } else if (security_rc == BLE_HS_EALREADY) {
        ESP_LOGI(kTag,"BLE security already active; handle=%u",connection_handle_);
    } else {
        ESP_LOGE(kTag,"BLE security initiation failed; handle=%u rc=%d",connection_handle_,security_rc);
    }
}

esp_err_t BleTransport::advertise() {
    if (ble_gap_adv_active() != 0) {
        ble_runtime_status::set_advertising(true);
        return ESP_OK;
    }

    ble_hs_adv_fields fields{};
    fields.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128=const_cast<ble_uuid128_t*>(&kServiceUuid);
    fields.num_uuids128=1;
    fields.uuids128_is_complete=1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ble_runtime_status::set_advertising(false);
        ESP_LOGE(kTag,"BLE advertising fields failed: rc=%d",rc);
        return ESP_FAIL;
    }

    // Keep the 128-bit HomeGuard service UUID in the advertisement itself and
    // place the human-readable controller name in the scan response. The UUID
    // plus full name does not fit in one legacy 31-byte advertising payload.
    // Android's ScanRecord combines the advertisement and scan response, so it
    // can now match by either the service UUID or HomeGuard-S3 device name.
    const char* name = ble_svc_gap_device_name();
    if (name != nullptr && name[0] != '\0') {
        ble_hs_adv_fields response{};
        response.name = reinterpret_cast<std::uint8_t*>(const_cast<char*>(name));
        response.name_len = std::strlen(name);
        response.name_is_complete = 1;
        rc = ble_gap_adv_rsp_set_fields(&response);
        if (rc != 0) {
            ble_runtime_status::set_advertising(false);
            ESP_LOGE(kTag,"BLE scan-response fields failed: rc=%d",rc);
            return ESP_FAIL;
        }
    }

    ble_gap_adv_params params{};
    params.conn_mode=BLE_GAP_CONN_MODE_UND;
    params.disc_mode=BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_address_type_,nullptr,BLE_HS_FOREVER,&params,gap_event,nullptr);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ble_runtime_status::set_advertising(false);
        ESP_LOGE(kTag,"BLE advertising start failed: rc=%d",rc);
        return ESP_FAIL;
    }

    const bool active = ble_gap_adv_active() != 0;
    ble_runtime_status::set_advertising(active);
    if (!active) {
        ESP_LOGE(kTag,"BLE advertising request returned rc=%d but advertising is not active",rc);
        return ESP_FAIL;
    }
    ESP_LOGI(kTag,"BLE advertising active; name=%s",name != nullptr ? name : "<none>");
    return ESP_OK;
}

esp_err_t BleTransport::scan_remotes() {
    ble_gap_disc_params params{};
    params.passive = 1;
    params.filter_duplicates = 0;
    const int rc = ble_gap_disc(own_address_type_, BLE_HS_FOREVER, &params, gap_event, nullptr);
    return rc == 0 || rc == BLE_HS_EALREADY ? ESP_OK : ESP_FAIL;
}

void BleTransport::accept_remote_advertisement(
    std::uint8_t address_type,
    const std::uint8_t address[6],
    const std::uint8_t* payload,
    std::size_t payload_size)
{
    if (remote_event_handler_ == nullptr || address == nullptr || payload == nullptr) return;

    // Native HomeGuard key-fob manufacturer payload:
    // optional 0xffff test/vendor prefix, "HGKF", version=1, action=0..5, uint32 LE replay counter.
    std::size_t offset = 0;
    if (payload_size >= kRemotePayloadBytes + 2U && payload[0] == 0xffU && payload[1] == 0xffU) offset = 2U;
    if (payload_size < offset + kRemotePayloadBytes) return;
    if (!std::equal(kRemoteMagic.begin(), kRemoteMagic.end(), payload + offset)) return;
    if (payload[offset + 4U] != kProtocolVersion) return;
    const auto action_value = payload[offset + 5U];
    if (action_value > static_cast<std::uint8_t>(hg::BleRemoteAction::Panic)) return;

    hg::BleRemoteEvent remote{};
    remote.identity.address_type = address_type;
    std::copy_n(address, remote.identity.address.size(), remote.identity.address.begin());
    remote.action = static_cast<hg::BleRemoteAction>(action_value);
    remote.counter_valid = true;
    remote.counter = static_cast<std::uint32_t>(payload[offset + 6U]) |
        (static_cast<std::uint32_t>(payload[offset + 7U]) << 8U) |
        (static_cast<std::uint32_t>(payload[offset + 8U]) << 16U) |
        (static_cast<std::uint32_t>(payload[offset + 9U]) << 24U);
    remote_event_handler_(remote, remote_event_context_);
}

void BleTransport::reset_rx() {
    rx_type_=0;
    rx_message_id_=0;
    rx_expected_count_=0;
    rx_next_index_=0;
    rx_payload_.clear();
}

int BleTransport::accept_rx_fragment(const std::uint8_t* data,std::size_t size) {
    if (!data || size<kHeaderSize || data[0]!=kProtocolVersion) return BLE_ATT_ERR_UNLIKELY;
    const auto type=data[1];
    const auto id=static_cast<std::uint16_t>(data[2])|(static_cast<std::uint16_t>(data[3])<<8U);
    const auto index=data[4];
    const auto count=data[5];
    if (!count || index>=count) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if (index==0) {
        reset_rx();
        rx_type_=type;
        rx_message_id_=id;
        rx_expected_count_=count;
    }
    if (type!=rx_type_ || id!=rx_message_id_ || count!=rx_expected_count_ || index!=rx_next_index_) {
        reset_rx();
        return BLE_ATT_ERR_UNLIKELY;
    }
    const auto n=size-kHeaderSize;
    if (rx_payload_.size()+n>kMaxMessageBytes) {
        reset_rx();
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    rx_payload_.append(reinterpret_cast<const char*>(data+kHeaderSize),n);
    ++rx_next_index_;
    if (rx_next_index_==rx_expected_count_) {
        const auto t=rx_type_;
        auto completed=std::move(rx_payload_);
        reset_rx();
        if (message_handler_) message_handler_(t,completed,message_context_);
    }
    return 0;
}

esp_err_t BleTransport::notify_message(std::uint8_t type,const std::string& payload) {
    if (!connected()) return ESP_ERR_INVALID_STATE;
    if (payload.size()>kMaxMessageBytes) return ESP_ERR_INVALID_SIZE;
    const auto mtu=static_cast<std::size_t>(ble_att_mtu(connection_handle_));
    const auto value_limit=std::min<std::size_t>(kMaxGattValue,mtu>3?mtu-3:20);
    if (value_limit<=kHeaderSize) return ESP_ERR_INVALID_SIZE;
    const auto payload_limit=value_limit-kHeaderSize;
    const auto fragments=std::max<std::size_t>(1,(payload.size()+payload_limit-1)/payload_limit);
    if (fragments>255) return ESP_ERR_INVALID_SIZE;
    const auto id=next_message_id_++;
    for (std::size_t index=0;index<fragments;++index) {
        const auto start=index*payload_limit;
        const auto n=start<payload.size()?std::min(payload_limit,payload.size()-start):0;
        std::array<std::uint8_t,kMaxGattValue> frame{};
        frame[0]=kProtocolVersion;
        frame[1]=type;
        frame[2]=static_cast<std::uint8_t>(id&0xffU);
        frame[3]=static_cast<std::uint8_t>((id>>8U)&0xffU);
        frame[4]=static_cast<std::uint8_t>(index);
        frame[5]=static_cast<std::uint8_t>(fragments);
        if (n) std::memcpy(frame.data()+kHeaderSize,payload.data()+start,n);
        auto* om=ble_hs_mbuf_from_flat(frame.data(),kHeaderSize+n);
        if (!om) return ESP_ERR_NO_MEM;
        if (ble_gatts_notify_custom(connection_handle_,g_tx_value_handle,om)!=0) return ESP_FAIL;
    }
    return ESP_OK;
}
}