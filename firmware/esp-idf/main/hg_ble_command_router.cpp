#include "hg_ble_command_router.hpp"

#include "hg_ble_transport.hpp"
#include "hg_http_util.hpp"
#include "hg_network_http.hpp"
#include "nvs_config_store.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/boot_readiness.hpp"
#include "homeguard/output_command.hpp"
#include "homeguard/physical_output_runtime.hpp"
#include "homeguard/system_model.hpp"

#include "esp_log.h"
#include "esp_timer.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "hg_ble_cmd";
constexpr std::uint8_t kCommandType = 3;
constexpr std::uint8_t kCommandReplyType = 4;
constexpr std::uint8_t kHelloSessionType = 5;
constexpr std::uint8_t kErrorType = 7;
constexpr std::uint8_t kProvisioningAuthorizeType = 8;
constexpr std::uint8_t kProvisioningApplyType = 9;
constexpr std::uint8_t kProvisioningReplyType = 10;

std::uint64_t now_ms()
{
    return static_cast<std::uint64_t>(esp_timer_get_time() / 1000);
}

const char* provisioning_reason(hg::ProvisioningCode code)
{
    switch (code) {
        case hg::ProvisioningCode::Accepted: return "accepted";
        case hg::ProvisioningCode::InvalidState: return "invalid_state";
        case hg::ProvisioningCode::Expired: return "expired";
        case hg::ProvisioningCode::InvalidProof: return "invalid_proof";
        case hg::ProvisioningCode::LockedOut: return "locked_out";
        case hg::ProvisioningCode::InvalidPayload: return "invalid_payload";
        case hg::ProvisioningCode::StorageFailure: return "storage_failure";
        case hg::ProvisioningCode::AlreadyProvisioned: return "already_provisioned";
    }
    return "unknown";
}

bool parse_uint16(const std::string& body, const char* key, std::uint16_t& value)
{
    const auto pos = http_util::value_offset(body, key);
    if (pos == std::string::npos) return false;
    const auto first = body.data() + pos;
    const auto last = body.data() + body.size();
    unsigned parsed{};
    const auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc{} || result.ptr == first || parsed > 65535U) return false;
    value = static_cast<std::uint16_t>(parsed);
    return true;
}

bool parse_uint32(const std::string& body, const char* key, std::uint32_t& value)
{
    const auto pos = http_util::value_offset(body, key);
    if (pos == std::string::npos) return false;
    const auto first = body.data() + pos;
    const auto last = body.data() + body.size();
    unsigned long parsed{};
    const auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc{} || result.ptr == first || parsed > 0xffffffffUL) return false;
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

bool parse_bool(const std::string& body, const char* key, bool& value)
{
    const auto pos = http_util::value_offset(body, key);
    if (pos == std::string::npos) return false;
    if (body.compare(pos, 4, "true") == 0) { value = true; return true; }
    if (body.compare(pos, 5, "false") == 0) { value = false; return true; }
    return false;
}

void wipe(std::string& value)
{
    std::fill(value.begin(), value.end(), '\0');
    value.clear();
}

const char* arm_state_name(hg::PartitionArmState state)
{
    switch (state) {
        case hg::PartitionArmState::Stay: return "stay";
        case hg::PartitionArmState::Away: return "away";
        case hg::PartitionArmState::Alarm: return "alarm";
        default: return "disarmed";
    }
}

std::string json_escape(std::string_view value)
{
    std::string out;
    out.reserve(value.size() + 8U);
    for (const char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) >= 0x20U) out.push_back(ch);
                break;
        }
    }
    return out;
}

template <std::size_t N>
std::string_view text_view(const std::array<char, N>& value)
{
    const auto end = std::find(value.begin(), value.end(), '\0');
    return {value.data(), static_cast<std::size_t>(end - value.begin())};
}

std::string capabilities_json(const homeguard::AccessControl& access, homeguard::AccessRole role)
{
    const auto allowed = [&](std::string_view command) {
        return access.role_allows(role, command) ? "true" : "false";
    };
    return std::string{"{\"monitor\":true,\"armHome\":"} + allowed("security.arm_home") +
           ",\"armAway\":" + allowed("security.arm_away") +
           ",\"disarm\":" + allowed("security.disarm") +
           ",\"panic\":" + allowed("security.panic") +
           ",\"valves\":" + allowed("valve.open") +
           ",\"networkConfigure\":" + allowed("network.configure") +
           ",\"accessManage\":" + allowed("access.manage") +
           ",\"serviceInvalidate\":" + allowed("system.service.invalidate") + "}";
}
}

void BleCommandRouter::configure(
    BleTransport* transport,
    homeguard::AccessControl* access,
    hg::SystemModel* model,
    hg::BootReadinessReport* readiness,
    hg::PhysicalOutputRuntime* physical,
    hg::SystemEventBus* bus,
    NetworkHttp* network,
    NvsConfigStore* provisioning_store)
{
    transport_ = transport;
    access_ = access;
    model_ = model;
    readiness_ = readiness;
    physical_ = physical;
    bus_ = bus;
    network_ = network;
    provisioning_store_ = provisioning_store;
    actor_.clear();
    authenticated_epoch_ = 0;
    provisioning_epoch_ = 0;
    provisioning_session_prepared_ = prepare_provisioning_session(now_ms());
    remote_runtime_.configure(model_, readiness_, physical_, bus_);
    if (transport_ != nullptr) transport_->set_remote_event_handler(&BleCommandRouter::on_remote_event, this);
}

void BleCommandRouter::on_remote_event(const hg::BleRemoteEvent& event, void* context)
{
    auto* self = static_cast<BleCommandRouter*>(context);
    if (self == nullptr) return;
    const auto result = self->remote_runtime_.ingest(event, now_ms());
    if (result.decision == hg::BleRemoteDecision::Accepted) {
        ESP_LOGI(kTag, "BLE keyfob action accepted: %u", static_cast<unsigned>(event.action));
    }
}

bool BleCommandRouter::session_valid() const
{
    return transport_ != nullptr && transport_->connected() && !actor_.empty() &&
           authenticated_epoch_ != 0U && authenticated_epoch_ == transport_->connection_epoch();
}

bool BleCommandRouter::prepare_provisioning_session(std::uint64_t current_ms)
{
    if (provisioning_store_ == nullptr || provisioning_store_->is_provisioned()) return false;

    FactoryProvisioningIdentity identity{};
    if (!provisioning_store_->load_factory_identity(identity)) {
        ESP_LOGE(kTag, "Factory provisioning identity unavailable or invalid");
        return false;
    }

    const auto code = provisioning_session_.begin(
        identity.pairing_code,
        identity.certificate_sha256,
        current_ms);
    identity.clear_private_material();
    if (code != hg::ProvisioningCode::Accepted) {
        ESP_LOGE(kTag, "Unable to prepare BLE provisioning session: %s", provisioning_reason(code));
        return false;
    }
    ESP_LOGI(kTag, "BLE factory provisioning window prepared");
    return true;
}

void BleCommandRouter::send(std::uint8_t type, const std::string& json) const
{
    if (transport_ == nullptr || !transport_->connected()) return;
    const auto error = transport_->send_message(type, json);
    if (error != ESP_OK) ESP_LOGW(kTag, "BLE reply failed: %s", esp_err_to_name(error));
}

void BleCommandRouter::send_error(const char* reason) const
{
    const std::string body = std::string{"{\"ok\":false,\"reason\":\""} +
        (reason == nullptr ? "unknown" : reason) + "\"}";
    send(kErrorType, body);
}

void BleCommandRouter::send_provisioning_reply(bool ok, const char* stage, const char* reason) const
{
    std::string body = std::string{"{\"ok\":"} + (ok ? "true" : "false") +
        ",\"stage\":\"" + (stage == nullptr ? "unknown" : stage) + "\"";
    if (reason != nullptr && reason[0] != '\0') {
        body += std::string{",\"reason\":\""} + reason + "\"";
    }
    body += "}";
    send(kProvisioningReplyType, body);
}

void BleCommandRouter::handle(std::uint8_t type, const std::string& json)
{
    if (type == kHelloSessionType) {
        handle_hello(json);
        return;
    }
    if (type == kCommandType) {
        handle_command(json);
        return;
    }
    if (type == kProvisioningAuthorizeType) {
        handle_provisioning_authorize(json);
        return;
    }
    if (type == kProvisioningApplyType) {
        handle_provisioning_apply(json);
        return;
    }
    send_error("unsupported_message_type");
}

void BleCommandRouter::handle_provisioning_authorize(const std::string& json)
{
    if (transport_ == nullptr || provisioning_store_ == nullptr) {
        send_provisioning_reply(false, "failed", "provisioning_unavailable");
        return;
    }
    if (provisioning_store_->is_provisioned()) {
        send_provisioning_reply(false, "failed", "already_provisioned");
        return;
    }

    const auto epoch = transport_->connection_epoch();
    const auto current_ms = now_ms();
    const auto status = provisioning_session_.status(current_ms);
    if (!provisioning_session_prepared_ ||
        ((status.state == hg::ProvisioningState::Authorized || status.state == hg::ProvisioningState::Applying) &&
         provisioning_epoch_ != epoch)) {
        provisioning_session_.abort();
        provisioning_session_prepared_ = prepare_provisioning_session(current_ms);
    }
    if (!provisioning_session_prepared_) {
        send_provisioning_reply(false, "failed", "factory_identity_unavailable");
        return;
    }

    std::string pairing_code;
    std::string certificate_sha256;
    if (!http_util::parse_json_string(json, "pairing_code", pairing_code) ||
        !http_util::parse_json_string(json, "certificate_sha256", certificate_sha256)) {
        wipe(pairing_code);
        send_provisioning_reply(false, "failed", "invalid_authorization_payload");
        return;
    }

    const auto code = provisioning_session_.authorize(pairing_code, certificate_sha256, current_ms);
    wipe(pairing_code);
    wipe(certificate_sha256);
    if (code != hg::ProvisioningCode::Accepted) {
        send_provisioning_reply(false, "failed", provisioning_reason(code));
        return;
    }

    provisioning_epoch_ = epoch;
    send_provisioning_reply(true, "authorized");
    ESP_LOGI(kTag, "BLE factory provisioning proof accepted for epoch=%lu",
             static_cast<unsigned long>(provisioning_epoch_));
}

void BleCommandRouter::handle_provisioning_apply(const std::string& json)
{
    if (transport_ == nullptr || network_ == nullptr || provisioning_store_ == nullptr ||
        provisioning_store_->is_provisioned()) {
        send_provisioning_reply(false, "failed",
            provisioning_store_ != nullptr && provisioning_store_->is_provisioned()
                ? "already_provisioned" : "provisioning_unavailable");
        return;
    }
    if (provisioning_epoch_ == 0U || provisioning_epoch_ != transport_->connection_epoch()) {
        send_provisioning_reply(false, "failed", "provisioning_authorization_required");
        return;
    }

    hg::ProvisioningPayload payload{};
    if (!http_util::parse_json_string(json, "wifi_ssid", payload.wifi_ssid) ||
        !http_util::parse_json_string(json, "wifi_password", payload.wifi_password) ||
        !http_util::parse_json_string(json, "local_api_token", payload.local_api_token)) {
        payload.clear_secrets();
        send_provisioning_reply(false, "failed", "invalid_payload");
        return;
    }
    (void)http_util::parse_json_string(json, "owner_label", payload.owner_label);
    (void)http_util::parse_json_string(json, "cloud_endpoint", payload.cloud_endpoint);
    (void)http_util::parse_json_string(json, "cloud_token", payload.cloud_token);

    const auto current_ms = now_ms();
    auto code = provisioning_session_.submit(std::move(payload), current_ms);
    if (code != hg::ProvisioningCode::Accepted || !provisioning_session_.pending()) {
        send_provisioning_reply(false, "failed", provisioning_reason(code));
        return;
    }

    const auto& pending = *provisioning_session_.pending();
    const bool provisioning_saved = provisioning_store_->save_provisioning(pending);
    const bool wifi_handover_started = provisioning_saved &&
        network_->provision_station(pending.wifi_ssid, pending.wifi_password);
    const bool storage_ok = provisioning_saved && wifi_handover_started;

    code = provisioning_session_.commit(storage_ok, now_ms());
    provisioning_epoch_ = 0;
    provisioning_session_prepared_ = false;
    if (code != hg::ProvisioningCode::Accepted) {
        send_provisioning_reply(false, "failed", provisioning_reason(code));
        ESP_LOGE(kTag, "BLE provisioning commit failed: %s", provisioning_reason(code));
        return;
    }

    send_provisioning_reply(true, "applied");
    ESP_LOGI(kTag, "BLE factory provisioning committed; Wi-Fi STA handover started");
}

void BleCommandRouter::handle_hello(const std::string& json)
{
    if (transport_ == nullptr || access_ == nullptr) {
        send_error("access_unavailable");
        return;
    }

    std::string actor;
    std::string pin;
    if (!http_util::parse_json_string(json, "actor", actor) || actor.empty() ||
        !http_util::parse_json_string(json, "pin", pin) || pin.empty()) {
        wipe(pin);
        send_error("invalid_session_hello");
        return;
    }

    const auto decision = access_->authenticate(actor, pin);
    wipe(pin);
    if (decision != homeguard::AuditDecision::Allowed) {
        actor_.clear();
        authenticated_epoch_ = 0;
        const std::string body = std::string{"{\"ok\":false,\"reason\":\""} +
            homeguard::to_string(decision) + "\"}";
        send(kHelloSessionType, body);
        return;
    }

    const auto* user = access_->find_user(actor);
    if (user == nullptr || !user->enabled) {
        actor_.clear();
        authenticated_epoch_ = 0;
        send(kHelloSessionType, "{\"ok\":false,\"reason\":\"user_unavailable\"}");
        return;
    }

    actor_ = actor;
    authenticated_epoch_ = transport_->connection_epoch();
    const std::string body = std::string{"{\"ok\":true,\"state\":\"authenticated\",\"actor\":\""} +
        json_escape(actor_) + "\",\"name\":\"" + json_escape(text_view(user->name)) +
        "\",\"role\":\"" + homeguard::to_string(user->role) +
        "\",\"capabilities\":" + capabilities_json(*access_, user->role) + "}";
    send(kHelloSessionType, body);
    ESP_LOGI(kTag, "BLE session authenticated for actor=%s role=%s epoch=%lu",
             actor_.c_str(), homeguard::to_string(user->role), static_cast<unsigned long>(authenticated_epoch_));
}

void BleCommandRouter::handle_command(const std::string& json)
{
    if (!session_valid()) {
        actor_.clear();
        authenticated_epoch_ = 0;
        send_error("ble_session_required");
        return;
    }
    if (access_ == nullptr || model_ == nullptr || readiness_ == nullptr || physical_ == nullptr || bus_ == nullptr) {
        send_error("runtime_unavailable");
        return;
    }

    std::string actor;
    std::string command;
    if (!http_util::parse_json_string(json, "actor", actor) || actor != actor_ ||
        !http_util::parse_json_string(json, "command", command) || command.empty()) {
        send(kCommandReplyType, "{\"ok\":false,\"reason\":\"invalid_command\"}");
        return;
    }

    if (command == "remote.pair_begin" || command == "remote.pair_cancel") {
        const auto decision = access_->authorize_session(actor_, "access.manage");
        if (decision != homeguard::AuditDecision::Allowed) {
            const std::string body = std::string{"{\"ok\":false,\"reason\":\""} +
                homeguard::to_string(decision) + "\"}";
            send(kCommandReplyType, body);
            return;
        }

        if (command == "remote.pair_cancel") {
            remote_runtime_.cancel_pairing();
            send(kCommandReplyType, "{\"ok\":true,\"command\":\"remote.pair_cancel\",\"pairing\":false}");
            return;
        }

        std::string name;
        std::uint32_t permissions{};
        if (!http_util::parse_json_string(json, "name", name) || name.empty() || name.size() > 23U ||
            !parse_uint32(json, "permissions", permissions) || permissions == 0U) {
            send(kCommandReplyType, "{\"ok\":false,\"reason\":\"invalid_remote_pairing\"}");
            return;
        }
        if (!remote_runtime_.begin_pairing(name, permissions, now_ms())) {
            send(kCommandReplyType, "{\"ok\":false,\"reason\":\"remote_pairing_unavailable\"}");
            return;
        }
        const std::string body = std::string{"{\"ok\":true,\"command\":\"remote.pair_begin\",\"pairing\":true,\"windowMs\":"} +
            std::to_string(BleRemoteRuntime::pairing_window_ms) +
            ",\"remoteCount\":" + std::to_string(remote_runtime_.size()) + "}";
        send(kCommandReplyType, body);
        return;
    }

    if (command == "security.arm_away" || command == "security.arm_home" ||
        command == "security.disarm" || command == "security.panic") {
        const auto decision = access_->authorize_session(actor_, command);
        if (decision != homeguard::AuditDecision::Allowed) {
            const std::string body = std::string{"{\"ok\":false,\"reason\":\""} +
                homeguard::to_string(decision) + "\"}";
            send(kCommandReplyType, body);
            return;
        }

        hg::PartitionArmState target = hg::PartitionArmState::Disarmed;
        if (command == "security.arm_away") target = hg::PartitionArmState::Away;
        else if (command == "security.arm_home") target = hg::PartitionArmState::Stay;
        else if (command == "security.panic") target = hg::PartitionArmState::Alarm;

        if (!model_->set_partition_arm(1, target, 0)) {
            send(kCommandReplyType, "{\"ok\":false,\"reason\":\"partition_command_failed\"}");
            return;
        }
        (void)bus_->dispatch_all();
        const std::string body = std::string{"{\"ok\":true,\"command\":\""} + command +
            "\",\"armState\":\"" + arm_state_name(target) + "\"}";
        send(kCommandReplyType, body);
        return;
    }

    if (command == "output.control") {
        std::uint16_t output_id{};
        bool active{};
        bool alarm_active{};
        if (!parse_uint16(json, "outputId", output_id) || !parse_bool(json, "active", active)) {
            send(kCommandReplyType, "{\"ok\":false,\"reason\":\"invalid_output_command\"}");
            return;
        }
        (void)parse_bool(json, "alarmActive", alarm_active);

        const auto* output = model_->output(output_id);
        if (output == nullptr) {
            send(kCommandReplyType, "{\"ok\":false,\"reason\":\"unknown_output\"}");
            return;
        }
        const std::string policy_command = output->type == hg::ModelOutputType::Valve
            ? (active ? "valve.open" : "valve.close")
            : "output.control";
        const auto decision = access_->authorize_session(actor_, policy_command);
        if (decision != homeguard::AuditDecision::Allowed) {
            const std::string body = std::string{"{\"ok\":false,\"reason\":\""} +
                homeguard::to_string(decision) + "\"}";
            send(kCommandReplyType, body);
            return;
        }

        const auto result = hg::apply_output_command(
            *model_, *readiness_, {output_id, active, alarm_active, 0});
        if (result.status == hg::OutputCommandStatus::Applied && !physical_->synchronize(*model_, *readiness_)) {
            (void)model_->set_output_active(output_id, false, 0);
            (void)physical_->force_safe();
            send(kCommandReplyType, "{\"ok\":false,\"reason\":\"physical_output_failure\",\"active\":false}");
            return;
        }

        if (result.status == hg::OutputCommandStatus::Applied) {
            (void)bus_->publish({hg::SystemEventType::ConfigChanged, output_id, 0, 0, active ? 5401 : 5400});
            (void)bus_->dispatch_all();
        }
        const std::string body = std::string{"{\"ok\":"} +
            (result.status == hg::OutputCommandStatus::Applied ? "true" : "false") +
            ",\"status\":\"" + hg::to_string(result.status) +
            "\",\"interlock\":\"" + hg::to_string(result.interlock) +
            "\",\"active\":" + (result.resulting_active ? "true" : "false") + "}";
        send(kCommandReplyType, body);
        return;
    }

    send(kCommandReplyType, "{\"ok\":false,\"reason\":\"unsupported_command\"}");
}

}  // namespace homeguard::idf
