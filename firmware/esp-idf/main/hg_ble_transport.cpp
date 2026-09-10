#include "hg_ble_transport.hpp"

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

homeguard::idf::BleTransport* g_owner = nullptr;
bool g_ready = false;
std::uint16_t g_tx_value_handle = 0;

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

int tx_access(std::uint16_t, std::uint16_t, ble_gatt_access_ctxt*, void*) {
    // NimBLE requires every registered characteristic to provide a non-null
    // access callback, including notify-only values. TX is delivered only via
    // ble_gatts_notify_custom(), so direct attribute access is rejected.
    return BLE_ATT_ERR_UNLIKELY;
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
            if (event->connect.status == 0) g_owner->on_connected(event->connect.conn_handle);
            else (void)g_owner->advertise();
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(kTag,"BLE link disconnected; reason=%d",event->disconnect.reason);
            g_owner->on_disconnected();
            (void)g_owner->advertise();
            break;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            (void)g_owner->advertise();
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == g_tx_value_handle) g_owner->on_notify_subscription(event->subscribe.cur_notify != 0);
            break;
        default: break;
    }
    return 0;
}

void stack_sync() {
    if (!g_owner) return;
    if (ble_hs_id_infer_auto(0,g_owner->own_address_type_storage()) != 0) {
        ESP_LOGE(kTag,"Cannot infer BLE identity address");
        return;
    }
    const auto error = g_owner->advertise();
    if (error != ESP_OK) ESP_LOGE(kTag,"BLE advertising failed: %s",esp_err_to_name(error));
}

void host_task(void*) {
    nimble_port_run();
    g_ready = false;
    vTaskDelete(nullptr);
}
}

namespace homeguard::idf {

bool ble_transport_ready() noexcept {
    return g_ready;
}

bool ble_transport_connected() noexcept {
    return g_ready && g_owner != nullptr && g_owner->connected();
}

esp_err_t BleTransport::start(const char* device_name) {
    if (!device_name || !device_name[0]) return ESP_ERR_INVALID_ARG;
    if (g_owner && g_owner != this) return ESP_ERR_INVALID_STATE;
    g_owner = this;
    g_ready = false;
    auto error = nimble_port_init();
    if (error != ESP_OK) return error;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    const int name_rc = ble_svc_gap_device_name_set(device_name);
    if (name_rc != 0) {
        ESP_LOGE(kTag,"BLE GAP device-name registration failed: rc=%d",name_rc);
        return ESP_FAIL;
    }
    int rc = ble_gatts_count_cfg(kServices);
    if (rc != 0) {
        ESP_LOGE(kTag,"BLE GATT resource validation failed: rc=%d",rc);
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
    ble_store_config_init();
    if (xTaskCreate(host_task,"hg_ble_host",4096,nullptr,5,nullptr) != pdPASS) return ESP_ERR_NO_MEM;
    g_ready = true;
    ESP_LOGI(kTag,"NimBLE HomeGuard transport started");
    return ESP_OK;
}

void BleTransport::set_message_handler(MessageHandler handler, void* context) {
    message_handler_=handler;
    message_context_=context;
}

bool BleTransport::connected() const {
    return connection_handle_ != BLE_HS_CONN_HANDLE_NONE && notify_enabled_;
}

esp_err_t BleTransport::publish_telemetry(const hg::TelemetryFrame& frame) {
    return notify_message(kTelemetryType,hg::telemetry_json(frame));
}

std::uint8_t* BleTransport::own_address_type_storage() {
    return &own_address_type_;
}

void BleTransport::on_connected(std::uint16_t handle) {
    connection_handle_=handle;
    notify_enabled_=false;
    reset_rx();
    ESP_LOGI(kTag,"Android BLE link connected; handle=%u",handle);
}

void BleTransport::on_disconnected() {
    connection_handle_=BLE_HS_CONN_HANDLE_NONE;
    notify_enabled_=false;
    reset_rx();
}

void BleTransport::on_notify_subscription(bool enabled) {
    notify_enabled_=enabled;
    ESP_LOGI(kTag,"BLE telemetry notifications %s",enabled?"enabled":"disabled");
}

esp_err_t BleTransport::advertise() {
    ble_hs_adv_fields fields{};
    fields.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128=const_cast<ble_uuid128_t*>(&kServiceUuid);
    fields.num_uuids128=1;
    fields.uuids128_is_complete=1;
    if (ble_gap_adv_set_fields(&fields) != 0) return ESP_FAIL;
    ble_gap_adv_params params{};
    params.conn_mode=BLE_GAP_CONN_MODE_UND;
    params.disc_mode=BLE_GAP_DISC_MODE_GEN;
    return ble_gap_adv_start(own_address_type_,nullptr,BLE_HS_FOREVER,&params,gap_event,nullptr)==0 ? ESP_OK : ESP_FAIL;
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

}  // namespace homeguard::idf
