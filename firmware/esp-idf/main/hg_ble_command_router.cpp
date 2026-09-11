#include "hg_ble_command_router.hpp"

#include "hg_ble_transport.hpp"
#include "hg_http_util.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/boot_readiness.hpp"
#include "homeguard/output_command.hpp"
#include "homeguard/physical_output_runtime.hpp"
#include "homeguard/system_model.hpp"

#include "esp_log.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string>

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "hg_ble_cmd";
constexpr std::uint8_t kCommandType = 3;
constexpr std::uint8_t kCommandReplyType = 4;
constexpr std::uint8_t kHelloSessionType = 5;
constexpr std::uint8_t kErrorType = 7;

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

bool parse_bool(const std::string& body, const char* key, bool& value)
{
    const auto pos = http_util::value_offset(body, key);
    if (pos == std::string::npos) return false;
    if (body.compare(pos, 4, "true") == 0) { value = true; return true; }
    if (body.compare(pos, 5, "false") == 0) { value = false; return true; }
    return false;
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
}

void BleCommandRouter::configure(
    BleTransport* transport,
    homeguard::AccessControl* access,
    hg::SystemModel* model,
    hg::BootReadinessReport* readiness,
    hg::PhysicalOutputRuntime* physical,
    hg::SystemEventBus* bus)
{
    transport_ = transport;
    access_ = access;
    model_ = model;
    readiness_ = readiness;
    physical_ = physical;
    bus_ = bus;
    actor_.clear();
    authenticated_epoch_ = 0;
}

bool BleCommandRouter::session_valid() const
{
    return transport_ != nullptr && transport_->connected() && !actor_.empty() &&
           authenticated_epoch_ != 0U && authenticated_epoch_ == transport_->connection_epoch();
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
    send_error("unsupported_message_type");
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
        std::fill(pin.begin(), pin.end(), '\0');
        send_error("invalid_session_hello");
        return;
    }

    const auto decision = access_->authenticate(actor, pin);
    std::fill(pin.begin(), pin.end(), '\0');
    pin.clear();
    if (decision != homeguard::AuditDecision::Allowed) {
        actor_.clear();
        authenticated_epoch_ = 0;
        const std::string body = std::string{"{\"ok\":false,\"reason\":\""} +
            homeguard::to_string(decision) + "\"}";
        send(kHelloSessionType, body);
        return;
    }

    actor_ = actor;
    authenticated_epoch_ = transport_->connection_epoch();
    send(kHelloSessionType, "{\"ok\":true,\"state\":\"authenticated\"}");
    ESP_LOGI(kTag, "BLE session authenticated for actor=%s epoch=%lu",
             actor_.c_str(), static_cast<unsigned long>(authenticated_epoch_));
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
