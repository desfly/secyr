#include "hg_service_http.hpp"
#include "hg_board_hw678.hpp"
#include "hg_hardware_bootstrap.hpp"
#include "homeguard/service_readiness.hpp"

#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace homeguard::idf {
namespace {

ServiceHttp* self_from(httpd_req_t* request) {
    return static_cast<ServiceHttp*>(request->user_ctx);
}

bool parse_json_string(const std::string& body, const char* key, std::string& value) {
    const std::string marker = std::string{"\""} + key + "\"";
    auto pos = body.find(marker);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + marker.size());
    if (pos == std::string::npos) return false;
    pos = body.find('"', pos + 1U);
    if (pos == std::string::npos) return false;
    const auto end = body.find('"', pos + 1U);
    if (end == std::string::npos) return false;
    value.assign(body, pos + 1U, end - pos - 1U);
    return true;
}

bool parse_json_bool(const std::string& body, const char* key, bool& value) {
    const std::string marker = std::string{"\""} + key + "\"";
    auto pos = body.find(marker);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + marker.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t' || body[pos] == '\r' || body[pos] == '\n')) ++pos;
    if (body.compare(pos, 4, "true") == 0) { value = true; return true; }
    if (body.compare(pos, 5, "false") == 0) { value = false; return true; }
    return false;
}

bool parse_json_u32(const std::string& body, const char* key, std::uint32_t& value) {
    const std::string marker = std::string{"\""} + key + "\"";
    auto pos = body.find(marker);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + marker.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t' || body[pos] == '\r' || body[pos] == '\n')) ++pos;
    const char* first = body.data() + pos;
    const char* last = body.data() + body.size();
    std::uint32_t parsed{};
    const auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc{}) return false;
    value = parsed;
    return true;
}

bool read_body(httpd_req_t* request, std::size_t limit, std::string& body) {
    if (request == nullptr || request->content_len == 0 || request->content_len > limit) return false;
    body.assign(request->content_len, '\0');
    std::size_t offset = 0;
    while (offset < body.size()) {
        const auto received = httpd_req_recv(request, body.data() + offset, body.size() - offset);
        if (received <= 0) return false;
        offset += static_cast<std::size_t>(received);
    }
    return true;
}

bool authorize_body(
    homeguard::AccessControl* access,
    const std::string& body,
    const char* command,
    homeguard::AuditDecision& decision)
{
    if (access == nullptr) return false;
    std::string actor;
    std::string credential;
    if (!parse_json_string(body, "actor", actor) ||
        !parse_json_string(body, "credential", credential)) {
        return false;
    }
    decision = access->authorize(actor, credential, command);
    std::fill(credential.begin(), credential.end(), '\0');
    return true;
}

std::uint64_t now_ms() noexcept {
    const auto value = static_cast<std::uint64_t>(esp_timer_get_time() / 1000);
    return value == 0U ? 1U : value;
}

void bench_delay(std::uint32_t duration_ms) {
    TickType_t ticks = pdMS_TO_TICKS(duration_ms);
    if (ticks == 0) ticks = 1;
    vTaskDelay(ticks);
}

bool fixed_hw678_record(hg::HardwareVerificationRecord& record) {
    record = {};
    record.pins.i2c_sda = static_cast<int>(board::kI2cSda);
    record.pins.i2c_scl = static_cast<int>(board::kI2cScl);
    record.pins.w5500_mosi = static_cast<int>(board::kW5500Mosi);
    record.pins.w5500_miso = static_cast<int>(board::kW5500Miso);
    record.pins.w5500_sclk = static_cast<int>(board::kW5500Sck);
    record.pins.w5500_cs = static_cast<int>(board::kW5500Cs);
    record.pins.w5500_int = static_cast<int>(board::kW5500Interrupt);
    record.pins.w5500_rst = static_cast<int>(board::kW5500Reset);
    record.pins.service_button = static_cast<int>(board::kServiceButton);
    record.active_polarity_verified = true;
    record.verified_at_ms = now_ms();
    record.profile_crc32 = hg::hardware_profile_crc32(record);
    return hg::validate_hardware_verification(record) == hg::HardwareVerificationStatus::Valid;
}

bool live_mcp_ready(const HardwareBootstrap& hardware) {
    const auto& status = hardware.status();
    return status.i2c.state == HardwareModuleState::Ready &&
           status.mcp23017.state == HardwareModuleState::Ready &&
           status.safe_outputs_forced;
}

bool dry_run_hardware_ready(const HardwareBootstrap& hardware) {
    return live_mcp_ready(hardware);
}

bool target_channel(
    const std::string& target,
    hg::PhysicalOutputChannel& channel,
    std::uint8_t& valve_mask)
{
    valve_mask = 0U;
    if (target == "light") { channel = hg::PhysicalOutputChannel::CorridorLight; return true; }
    if (target == "siren") { channel = hg::PhysicalOutputChannel::Siren; return true; }
    if (target == "cold_open") { channel = hg::PhysicalOutputChannel::ColdValveOpen; valve_mask = 0x01U; return true; }
    if (target == "cold_close") { channel = hg::PhysicalOutputChannel::ColdValveClose; valve_mask = 0x02U; return true; }
    if (target == "hot_open") { channel = hg::PhysicalOutputChannel::HotValveOpen; valve_mask = 0x04U; return true; }
    if (target == "hot_close") { channel = hg::PhysicalOutputChannel::HotValveClose; valve_mask = 0x08U; return true; }
    return false;
}

}  // namespace

esp_err_t ServiceHttp::register_handlers(
    httpd_handle_t server,
    CommissioningNvsStore* store,
    hg::HardwareVerificationRecord* hardware,
    hg::CommissioningPersistentState* commissioning,
    hg::BootReadinessReport* readiness,
    hg::PhysicalOutputRuntime* physical_outputs,
    hg::SystemEventBus* bus,
    hg::SystemModel* model,
    HardwareBootstrap* hardware_runtime,
    std::mutex* control_state_mutex)
{
    if (server == nullptr || store == nullptr || hardware == nullptr || commissioning == nullptr ||
        readiness == nullptr || physical_outputs == nullptr || bus == nullptr || model == nullptr ||
        hardware_runtime == nullptr || control_state_mutex == nullptr) return ESP_ERR_INVALID_ARG;

    store_ = store;
    hardware_ = hardware;
    commissioning_ = commissioning;
    readiness_ = readiness;
    physical_outputs_ = physical_outputs;
    bus_ = bus;
    model_ = model;
    hardware_runtime_ = hardware_runtime;
    control_state_mutex_ = control_state_mutex;

    const httpd_uri_t routes[] = {
        {.uri="/api/v1/service/readiness", .method=HTTP_GET, .handler=&ServiceHttp::readiness_get, .user_ctx=this},
        {.uri="/api/v1/service/maintenance", .method=HTTP_POST, .handler=&ServiceHttp::maintenance_post, .user_ctx=this},
        {.uri="/api/v1/service/commissioning/hardware-verify", .method=HTTP_POST, .handler=&ServiceHttp::hardware_verify_post, .user_ctx=this},
        {.uri="/api/v1/service/commissioning/dry-run", .method=HTTP_POST, .handler=&ServiceHttp::dry_run_post, .user_ctx=this},
        {.uri="/api/v1/service/commissioning/valve-profile", .method=HTTP_POST, .handler=&ServiceHttp::valve_profile_post, .user_ctx=this},
        {.uri="/api/v1/service/commissioning/bench-pulse", .method=HTTP_POST, .handler=&ServiceHttp::bench_pulse_post, .user_ctx=this},
        {.uri="/api/v1/service/commissioning/actuator-accept", .method=HTTP_POST, .handler=&ServiceHttp::actuator_accept_post, .user_ctx=this},
        {.uri="/api/v1/service/invalidate", .method=HTTP_POST, .handler=&ServiceHttp::invalidate_post, .user_ctx=this},
        {.uri="/api/v1/service/factory-reset", .method=HTTP_POST, .handler=&ServiceHttp::factory_reset_post, .user_ctx=this},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

esp_err_t ServiceHttp::send_json(httpd_req_t* request, const std::string& body) const {
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

bool ServiceHttp::maintenance_active() const {
    if (control_state_mutex_ == nullptr) return false;
    std::scoped_lock lock(*control_state_mutex_);
    return maintenance_active_;
}

bool ServiceHttp::refresh_control_state_from_store() {
    if (store_ == nullptr || hardware_ == nullptr || commissioning_ == nullptr ||
        readiness_ == nullptr || physical_outputs_ == nullptr || control_state_mutex_ == nullptr) return false;

    hg::HardwareVerificationRecord hardware{};
    hg::CommissioningPersistentState commissioning{};
    const auto hardware_error = store_->load_hardware(hardware);
    const auto commissioning_error = store_->load_commissioning(commissioning);
    const auto* hardware_ptr = hardware_error == ESP_OK ? &hardware : nullptr;
    const auto* commissioning_ptr = commissioning_error == ESP_OK ? &commissioning : nullptr;
    const auto readiness = hg::evaluate_boot_readiness({hardware_ptr, commissioning_ptr});

    const auto hardware_copy = hardware_error == ESP_OK ? hardware : hg::HardwareVerificationRecord{};
    const auto commissioning_copy = commissioning_error == ESP_OK ? commissioning : hg::CommissioningPersistentState{};
    {
        std::scoped_lock lock(*control_state_mutex_);
        *hardware_ = hardware_copy;
        *commissioning_ = commissioning_copy;
        *readiness_ = readiness;
    }
    return physical_outputs_->update_control_state(hardware_copy, commissioning_copy, readiness);
}

esp_err_t ServiceHttp::readiness_get(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->store_ == nullptr || self->physical_outputs_ == nullptr ||
        self->hardware_runtime_ == nullptr || self->control_state_mutex_ == nullptr) return ESP_FAIL;

    hg::HardwareVerificationRecord hardware{};
    hg::CommissioningPersistentState commissioning{};
    const auto hardware_error = self->store_->load_hardware(hardware);
    const auto commissioning_error = self->store_->load_commissioning(commissioning);
    const auto snapshot = hg::make_service_readiness_snapshot(
        hardware_error == ESP_OK ? &hardware : nullptr,
        commissioning_error == ESP_OK ? &commissioning : nullptr);

    bool maintenance = false;
    std::uint8_t bench_mask = 0;
    {
        std::scoped_lock lock(*self->control_state_mutex_);
        maintenance = self->maintenance_active_;
        bench_mask = self->bench_valve_mask_;
    }
    const auto physical = self->physical_outputs_->state();
    std::string body = hg::service_readiness_json(snapshot);
    if (!body.empty() && body.back() == '}') body.pop_back();
    body += std::string{",\"maintenanceActive\":"} + (maintenance ? "true" : "false") +
        ",\"benchValveMask\":" + std::to_string(bench_mask) +
        ",\"physicalStatus\":\"" + hg::to_string(physical.status) + "\"" +
        ",\"hardwareOverall\":\"" + to_string(self->hardware_runtime_->status().overall) + "\"" +
        ",\"hardwareVerification\":" + (hardware_error == ESP_OK ? hg::hardware_verification_json(hardware) : "null") +
        ",\"commissioning\":" + (commissioning_error == ESP_OK ? hg::commissioning_state_json(commissioning) : "null") +
        "}";
    return self->send_json(request, body);
}

esp_err_t ServiceHttp::maintenance_post(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->physical_outputs_ == nullptr || self->model_ == nullptr ||
        self->control_state_mutex_ == nullptr) return ESP_FAIL;

    std::string body;
    if (!read_body(request, 384U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }
    bool active = false;
    if (!parse_json_bool(body, "active", active)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"active_required\"}");
    }
    homeguard::AuditDecision decision{};
    if (!authorize_body(self->access_control_, body, "system.commissioning.maintenance", decision)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"forbidden\"}");
    }

    if (!self->physical_outputs_->set_maintenance_mode(active, *self->model_)) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"physical_output_not_safe\"}");
    }
    {
        std::scoped_lock lock(*self->control_state_mutex_);
        self->maintenance_active_ = active;
        self->bench_valve_mask_ = 0U;
    }
    return self->send_json(request,
        std::string{"{\"ok\":true,\"maintenanceActive\":"} + (active ? "true" : "false") + "}");
}

esp_err_t ServiceHttp::hardware_verify_post(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->store_ == nullptr || self->hardware_runtime_ == nullptr ||
        self->control_state_mutex_ == nullptr) return ESP_FAIL;

    std::string body;
    if (!read_body(request, 512U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }
    homeguard::AuditDecision decision{};
    if (!authorize_body(self->access_control_, body, "system.commissioning.hardware_verify", decision)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"forbidden\"}");
    }
    std::string confirmation;
    if (!parse_json_string(body, "confirm", confirmation) || confirmation != "HW678_MCP23017_VERIFIED") {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"verification_confirmation_required\"}");
    }
    if (!self->maintenance_active()) {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"maintenance_required\"}");
    }
    if (!live_mcp_ready(*self->hardware_runtime_)) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"mcp23017_not_ready\"}");
    }

    hg::HardwareVerificationRecord hardware{};
    if (!fixed_hw678_record(hardware)) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"hardware_profile_invalid\"}");
    }
    auto error = self->store_->save_hardware(hardware);
    if (error != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"hardware_save_failed\"}");
    }

    hg::CommissioningPersistentState progress{};
    progress.gpio_map_verified = true;
    progress.active_polarity_verified = true;
    progress.last_verified_at_ms = now_ms();
    error = self->store_->save_commissioning(progress);
    if (error != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"commissioning_progress_save_failed\"}");
    }
    {
        std::scoped_lock lock(*self->control_state_mutex_);
        self->bench_valve_mask_ = 0U;
    }
    if (!self->refresh_control_state_from_store()) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"control_state_refresh_failed\"}");
    }
    return self->send_json(request,
        "{\"ok\":true,\"next\":\"dry_run\",\"outputsAllowed\":false}");
}

esp_err_t ServiceHttp::dry_run_post(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->store_ == nullptr || self->hardware_runtime_ == nullptr) return ESP_FAIL;

    std::string body;
    if (!read_body(request, 512U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }
    homeguard::AuditDecision decision{};
    if (!authorize_body(self->access_control_, body, "system.commissioning.dry_run", decision)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"forbidden\"}");
    }
    std::string confirmation;
    if (!parse_json_string(body, "confirm", confirmation) || confirmation != "DRY_RUN_PASS") {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"dry_run_confirmation_required\"}");
    }
    if (!self->maintenance_active()) {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"maintenance_required\"}");
    }
    if (!dry_run_hardware_ready(*self->hardware_runtime_)) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"dry_run_hardware_not_ready\"}");
    }

    hg::HardwareVerificationRecord hardware{};
    if (self->store_->load_hardware(hardware) != ESP_OK) {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"hardware_verification_required\"}");
    }
    hg::CommissioningPersistentState progress{};
    const auto load_error = self->store_->load_commissioning(progress);
    if (load_error != ESP_OK) {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"commissioning_progress_missing\"}");
    }

    progress.gpio_map_verified = true;
    progress.active_polarity_verified = true;
    if (progress.successful_dry_runs != std::numeric_limits<std::uint32_t>::max()) {
        ++progress.successful_dry_runs;
    }
    progress.last_verified_at_ms = now_ms();
    if (self->store_->save_commissioning(progress) != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"dry_run_save_failed\"}");
    }
    if (!self->refresh_control_state_from_store()) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"control_state_refresh_failed\"}");
    }
    return self->send_json(request,
        "{\"ok\":true,\"next\":\"valve_profile\",\"outputsAllowed\":false}");
}

esp_err_t ServiceHttp::valve_profile_post(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->store_ == nullptr || self->control_state_mutex_ == nullptr) return ESP_FAIL;

    std::string body;
    if (!read_body(request, 640U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }
    homeguard::AuditDecision decision{};
    if (!authorize_body(self->access_control_, body, "system.commissioning.valve_profile", decision)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"forbidden\"}");
    }
    std::string confirmation;
    bool active_low = false;
    std::uint32_t cold_timeout = 0;
    std::uint32_t hot_timeout = 0;
    if (!parse_json_string(body, "confirm", confirmation) || confirmation != "VALVE_PROFILE_VERIFIED" ||
        !parse_json_bool(body, "activeLow", active_low) ||
        !parse_json_u32(body, "coldTimeoutMs", cold_timeout) ||
        !parse_json_u32(body, "hotTimeoutMs", hot_timeout) ||
        cold_timeout == 0U || hot_timeout == 0U) {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"verified_valve_profile_required\"}");
    }
    if (!self->maintenance_active()) {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"maintenance_required\"}");
    }

    hg::CommissioningPersistentState progress{};
    if (self->store_->load_commissioning(progress) != ESP_OK || progress.successful_dry_runs == 0U) {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"dry_run_required\"}");
    }

    progress.valve_limit_polarity_verified = true;
    progress.valve_limits_active_low = active_low;
    progress.cold_valve_travel_timeout_ms = cold_timeout;
    progress.hot_valve_travel_timeout_ms = hot_timeout;
    progress.successful_actuator_tests = 0U;
    progress.last_verified_at_ms = now_ms();
    if (self->store_->save_commissioning(progress) != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"valve_profile_save_failed\"}");
    }
    {
        std::scoped_lock lock(*self->control_state_mutex_);
        self->bench_valve_mask_ = 0U;
    }
    if (!self->refresh_control_state_from_store()) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"control_state_refresh_failed\"}");
    }
    return self->send_json(request,
        "{\"ok\":true,\"next\":\"bench_valve_directions\",\"outputsAllowed\":false}");
}

esp_err_t ServiceHttp::bench_pulse_post(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->physical_outputs_ == nullptr || self->model_ == nullptr ||
        self->hardware_runtime_ == nullptr || self->control_state_mutex_ == nullptr) return ESP_FAIL;

    std::string body;
    if (!read_body(request, 640U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }
    homeguard::AuditDecision decision{};
    if (!authorize_body(self->access_control_, body, "system.commissioning.bench_pulse", decision)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"forbidden\"}");
    }
    std::string confirmation;
    std::string target;
    std::uint32_t duration_ms = 0;
    if (!parse_json_string(body, "confirm", confirmation) || confirmation != "BENCH_PULSE" ||
        !parse_json_string(body, "target", target) ||
        !parse_json_u32(body, "durationMs", duration_ms)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"bench_pulse_parameters_required\"}");
    }
    if (!self->maintenance_active()) {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"maintenance_required\"}");
    }
    if (!live_mcp_ready(*self->hardware_runtime_)) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"mcp23017_not_ready\"}");
    }
    hg::PartitionRecord partition{};
    if (!self->model_->partition_snapshot(1, partition) ||
        partition.arm_state != hg::PartitionArmState::Disarmed) {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"system_must_be_disarmed\"}");
    }

    hg::PhysicalOutputChannel channel{};
    std::uint8_t valve_mask = 0;
    if (!target_channel(target, channel, valve_mask) || duration_ms == 0U ||
        duration_ms > hg::PhysicalOutputRuntime::kMaxBenchPulseMs) {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"bench_pulse_out_of_range\"}");
    }

    if (!self->physical_outputs_->bench_pulse(channel, duration_ms, &bench_delay)) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"bench_pulse_failed\"}");
    }

    bool evidence_counted = false;
    if (valve_mask != 0U && self->store_ != nullptr) {
        hg::HardwareVerificationRecord hardware{};
        hg::CommissioningPersistentState progress{};
        const bool prerequisites =
            self->store_->load_hardware(hardware) == ESP_OK &&
            self->store_->load_commissioning(progress) == ESP_OK &&
            progress.successful_dry_runs > 0U &&
            progress.valve_limit_polarity_verified &&
            progress.cold_valve_travel_timeout_ms > 0U &&
            progress.hot_valve_travel_timeout_ms > 0U;
        if (prerequisites) {
            std::scoped_lock lock(*self->control_state_mutex_);
            self->bench_valve_mask_ |= valve_mask;
            evidence_counted = true;
        }
    }

    return self->send_json(request,
        std::string{"{\"ok\":true,\"target\":\""} + target +
        "\",\"durationMs\":" + std::to_string(duration_ms) +
        ",\"actuatorEvidenceCounted\":" + (evidence_counted ? "true" : "false") + "}");
}

esp_err_t ServiceHttp::actuator_accept_post(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->store_ == nullptr || self->control_state_mutex_ == nullptr) return ESP_FAIL;

    std::string body;
    if (!read_body(request, 512U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }
    homeguard::AuditDecision decision{};
    if (!authorize_body(self->access_control_, body, "system.commissioning.actuator_accept", decision)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"forbidden\"}");
    }
    std::string confirmation;
    if (!parse_json_string(body, "confirm", confirmation) || confirmation != "ACTUATOR_TEST_PASS") {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"actuator_confirmation_required\"}");
    }
    if (!self->maintenance_active()) {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"maintenance_required\"}");
    }

    std::uint8_t bench_mask = 0;
    {
        std::scoped_lock lock(*self->control_state_mutex_);
        bench_mask = self->bench_valve_mask_;
    }
    if (bench_mask != 0x0FU) {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"all_four_valve_directions_required\"}");
    }

    hg::CommissioningPersistentState progress{};
    if (self->store_->load_commissioning(progress) != ESP_OK ||
        progress.successful_dry_runs == 0U ||
        !progress.valve_limit_polarity_verified ||
        progress.cold_valve_travel_timeout_ms == 0U ||
        progress.hot_valve_travel_timeout_ms == 0U) {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"commissioning_prerequisites_incomplete\"}");
    }

    if (progress.successful_actuator_tests < progress.successful_dry_runs) {
        ++progress.successful_actuator_tests;
    }
    progress.last_verified_at_ms = now_ms();
    if (self->store_->save_commissioning(progress) != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"actuator_accept_save_failed\"}");
    }
    {
        std::scoped_lock lock(*self->control_state_mutex_);
        self->bench_valve_mask_ = 0U;
    }
    if (!self->refresh_control_state_from_store()) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"control_state_refresh_failed\"}");
    }

    return self->send_json(request,
        "{\"ok\":true,\"status\":\"actuator_test_accepted\",\"normalOutputsReadyAfterMaintenanceExit\":true}");
}

esp_err_t ServiceHttp::invalidate_post(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->store_ == nullptr || self->hardware_ == nullptr ||
        self->commissioning_ == nullptr || self->readiness_ == nullptr ||
        self->physical_outputs_ == nullptr || self->control_state_mutex_ == nullptr) return ESP_FAIL;

    std::string body;
    if (!read_body(request, 384U, body)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }
    homeguard::AuditDecision decision{};
    if (!authorize_body(self->access_control_, body, "system.service.invalidate", decision)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"forbidden\"}");
    }

    if (!self->physical_outputs_->lockout_fail_closed()) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"output_safe_failed\"}");
    }

    const auto error = self->store_->erase_all();
    if (error != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"nvs_erase_failed\"}");
    }

    {
        std::scoped_lock lock(*self->control_state_mutex_);
        *self->hardware_ = {};
        *self->commissioning_ = {};
        *self->readiness_ = hg::evaluate_boot_readiness({nullptr, nullptr});
        self->maintenance_active_ = false;
        self->bench_valve_mask_ = 0U;
    }
    if (self->bus_ != nullptr) {
        self->bus_->publish({hg::SystemEventType::ConfigChanged, 0, 0, 0, 5000});
    }

    const auto response_error = self->send_json(request,
        "{\"ok\":true,\"state\":\"restarting\",\"outputsAllowed\":false,\"reason\":\"commissioning_invalidated\"}");
    vTaskDelay(pdMS_TO_TICKS(150));
    esp_restart();
    return response_error;
}

esp_err_t ServiceHttp::factory_reset_post(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->hardware_ == nullptr || self->commissioning_ == nullptr ||
        self->readiness_ == nullptr || self->physical_outputs_ == nullptr ||
        self->control_state_mutex_ == nullptr) return ESP_FAIL;

    std::string body;
    if (!read_body(request, 512U, body)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }
    std::string confirmation;
    if (!parse_json_string(body, "confirm", confirmation) || confirmation != "ERASE_ALL") {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"confirmation_mismatch\"}");
    }
    homeguard::AuditDecision decision{};
    if (!authorize_body(self->access_control_, body, "system.factory_reset", decision)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"forbidden\"}");
    }

    if (!self->physical_outputs_->lockout_fail_closed()) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"output_safe_failed\"}");
    }

    const auto erase_error = nvs_flash_erase();
    if (erase_error != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"factory_reset_erase_failed\"}");
    }

    {
        std::scoped_lock lock(*self->control_state_mutex_);
        *self->hardware_ = {};
        *self->commissioning_ = {};
        *self->readiness_ = hg::evaluate_boot_readiness({nullptr, nullptr});
        self->maintenance_active_ = false;
        self->bench_valve_mask_ = 0U;
    }

    const auto response_error = self->send_json(request,
        "{\"ok\":true,\"state\":\"restarting\",\"factoryReset\":true,\"outputsAllowed\":false}");
    vTaskDelay(pdMS_TO_TICKS(150));
    esp_restart();
    return response_error;
}

}  // namespace homeguard::idf
