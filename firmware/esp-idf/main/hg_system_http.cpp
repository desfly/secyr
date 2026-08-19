#include "hg_system_http.hpp"
#include "hg_reset_sequence.hpp"
#include "hg_http_util.hpp"
#include "hg_request_auth.hpp"
#include "homeguard/system_api.hpp"

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

namespace homeguard::idf {

namespace {
SystemHttp* self_from(httpd_req_t* request) {
    return request == nullptr ? nullptr : static_cast<SystemHttp*>(request->user_ctx);
}

hg::Severity severity_for(hg::SystemEventType type) {
    switch (type) {
        case hg::SystemEventType::Alarm: return hg::Severity::Alarm;
        case hg::SystemEventType::Tamper:
        case hg::SystemEventType::BatteryLow:
        case hg::SystemEventType::SensorOffline: return hg::Severity::Warning;
        default: return hg::Severity::Info;
    }
}

const char* severity_name(hg::Severity severity) {
    switch (severity) {
        case hg::Severity::Warning: return "warning";
        case hg::Severity::Alarm: return "alarm";
        case hg::Severity::Fault: return "fault";
        default: return "info";
    }
}

void delayed_factory_reboot(void*) {
    vTaskDelay(pdMS_TO_TICKS(350));
    esp_restart();
}

void schedule_factory_reboot() {
    if (xTaskCreate(&delayed_factory_reboot, "hg_factory_reset", 2048, nullptr, 5, nullptr) != pdPASS) {
        esp_restart();
    }
}

const char* arm_state_name(hg::PartitionArmState state) {
    switch (state) {
        case hg::PartitionArmState::Stay: return "stay";
        case hg::PartitionArmState::Away: return "away";
        case hg::PartitionArmState::Alarm: return "alarm";
        default: return "disarmed";
    }
}
}

esp_err_t SystemHttp::register_handlers(
    httpd_handle_t server,
    hg::SystemModel* model,
    hg::SystemEventBus* bus,
    homeguard::AccessControl* access_control) {
    if (server == nullptr || model == nullptr || bus == nullptr || access_control == nullptr) return ESP_ERR_INVALID_ARG;
    server_ = server;
    model_ = model;
    bus_ = bus;
    access_control_ = access_control;
    if (!bus_->subscribe(&SystemHttp::on_event, this)) return ESP_ERR_NO_MEM;

    const httpd_uri_t routes[] = {
        {.uri="/api/v1/system/status", .method=HTTP_GET, .handler=&SystemHttp::status_get, .user_ctx=this},
        {.uri="/api/v1/system/zones", .method=HTTP_GET, .handler=&SystemHttp::zones_get, .user_ctx=this},
        {.uri="/api/v1/system/outputs", .method=HTTP_GET, .handler=&SystemHttp::outputs_get, .user_ctx=this},
        {.uri="/api/v1/system/partitions", .method=HTTP_GET, .handler=&SystemHttp::partitions_get, .user_ctx=this},
        {.uri="/api/v1/system/events", .method=HTTP_GET, .handler=&SystemHttp::events_get, .user_ctx=this},
        {.uri="/api/v1/system/security-command", .method=HTTP_POST, .handler=&SystemHttp::security_command_post, .user_ctx=this},
        {.uri="/api/v1/system/factory-reset", .method=HTTP_POST, .handler=&SystemHttp::factory_reset_post, .user_ctx=this},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server_, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

bool SystemHttp::authenticated_request(httpd_req_t* request) const {
    return access_control_ != nullptr && request_auth::authenticated(request, *access_control_);
}

esp_err_t SystemHttp::send_json(httpd_req_t* request, const char* body, std::size_t size) const {
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body, static_cast<ssize_t>(size));
}

esp_err_t SystemHttp::status_get(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->model_ == nullptr || self->bus_ == nullptr) return ESP_FAIL;
    if (!self->authenticated_request(request)) return request_auth::send_login_required(request);
    const auto body = hg::system_status_json(*self->model_, *self->bus_);
    return self->send_json(request, body.c_str(), body.size());
}

esp_err_t SystemHttp::zones_get(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->model_ == nullptr) return ESP_FAIL;
    if (!self->authenticated_request(request)) return request_auth::send_login_required(request);
    const auto body = hg::system_zones_json(*self->model_);
    return self->send_json(request, body.c_str(), body.size());
}

esp_err_t SystemHttp::outputs_get(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->model_ == nullptr) return ESP_FAIL;
    if (!self->authenticated_request(request)) return request_auth::send_login_required(request);
    const auto body = hg::system_outputs_json(*self->model_);
    return self->send_json(request, body.c_str(), body.size());
}

esp_err_t SystemHttp::partitions_get(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->model_ == nullptr) return ESP_FAIL;
    if (!self->authenticated_request(request)) return request_auth::send_login_required(request);
    const auto body = hg::system_partitions_json(*self->model_);
    return self->send_json(request, body.c_str(), body.size());
}

esp_err_t SystemHttp::events_get(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr) return ESP_FAIL;
    if (!self->authenticated_request(request)) return request_auth::send_login_required(request);
    const auto body = self->events_json();
    return self->send_json(request, body.c_str(), body.size());
}

esp_err_t SystemHttp::security_command_post(httpd_req_t* request) {
    auto* self = self_from(request);
    return self ? self->handle_security_command(request) : ESP_FAIL;
}

esp_err_t SystemHttp::factory_reset_post(httpd_req_t* request) {
    auto* self = self_from(request);
    return self ? self->handle_factory_reset(request) : ESP_FAIL;
}

esp_err_t SystemHttp::handle_factory_reset(httpd_req_t* request) {
    if (access_control_ == nullptr) return ESP_FAIL;
    std::string body;
    if (!http_util::read_body(request, 512U, body)) {
        httpd_resp_set_status(request,"400 Bad Request");
        return httpd_resp_send(request,"{\"ok\":false,\"reason\":\"invalid_body\"}",-1);
    }
    std::string actor, credential, confirm;
    if (!http_util::parse_json_string(body,"actor",actor) || !http_util::parse_json_string(body,"credential",credential)) {
        http_util::scrub(credential); http_util::scrub(body);
        httpd_resp_set_status(request,"401 Unauthorized");
        return httpd_resp_send(request,"{\"ok\":false,\"reason\":\"credential_required\"}",-1);
    }
    if (!http_util::parse_json_string(body,"confirm",confirm) || confirm != "ERASE_ALL") {
        http_util::scrub(credential); http_util::scrub(body);
        httpd_resp_set_status(request,"409 Conflict");
        return httpd_resp_send(request,"{\"ok\":false,\"reason\":\"explicit_confirmation_required\"}",-1);
    }
    http_util::scrub(body);
    const auto decision = access_control_->authorize(actor,credential,"system.factory_reset");
    http_util::scrub(credential);
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request,"403 Forbidden");
        const std::string response = std::string{"{\"ok\":false,\"reason\":\""} + homeguard::to_string(decision) + "\"}";
        return send_json(request,response.c_str(),response.size());
    }

    const auto stage_error = stage_factory_reset_request();
    if (stage_error != ESP_OK) {
        httpd_resp_set_status(request,"500 Internal Server Error");
        const std::string response = std::string{"{\"ok\":false,\"reason\":\"stage_failed\",\"error\":"} +
            std::to_string(stage_error) + "}";
        return send_json(request,response.c_str(),response.size());
    }

    schedule_factory_reboot();
    static constexpr char response[] = "{\"ok\":true,\"state\":\"factory_reset_staged\",\"rebooting\":true}";
    return send_json(request,response,sizeof(response)-1U);
}

esp_err_t SystemHttp::handle_security_command(httpd_req_t* request) {
    if (model_ == nullptr || bus_ == nullptr || access_control_ == nullptr) return ESP_FAIL;
    std::string body;
    if (!http_util::read_body(request,384U,body)) {
        httpd_resp_set_status(request,"400 Bad Request");
        return httpd_resp_send(request,"{\"ok\":false,\"reason\":\"invalid_body\"}",-1);
    }
    std::string command,actor,credential;
    if (!http_util::parse_json_string(body,"command",command)) {
        http_util::scrub(body);
        httpd_resp_set_status(request,"400 Bad Request");
        return httpd_resp_send(request,"{\"ok\":false,\"reason\":\"missing_command\"}",-1);
    }
    if (!http_util::parse_json_string(body,"actor",actor) || !http_util::parse_json_string(body,"credential",credential)) {
        http_util::scrub(credential); http_util::scrub(body);
        httpd_resp_set_status(request,"401 Unauthorized");
        return httpd_resp_send(request,"{\"ok\":false,\"reason\":\"credential_required\"}",-1);
    }
    http_util::scrub(body);
    const auto decision = access_control_->authorize(actor,credential,command);
    http_util::scrub(credential);
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request,"403 Forbidden");
        const std::string response = std::string{"{\"ok\":false,\"reason\":\""} + homeguard::to_string(decision) + "\"}";
        return send_json(request,response.c_str(),response.size());
    }

    hg::PartitionArmState target{};
    if (command == "security.arm_away") target = hg::PartitionArmState::Away;
    else if (command == "security.arm_home") target = hg::PartitionArmState::Stay;
    else if (command == "security.disarm") target = hg::PartitionArmState::Disarmed;
    else if (command == "security.panic") target = hg::PartitionArmState::Alarm;
    else {
        httpd_resp_set_status(request,"400 Bad Request");
        return httpd_resp_send(request,"{\"ok\":false,\"reason\":\"unsupported_command\"}",-1);
    }

    if (!model_->set_partition_arm(1,target,0)) {
        httpd_resp_set_status(request,"409 Conflict");
        return httpd_resp_send(request,"{\"ok\":false,\"reason\":\"partition_command_failed\"}",-1);
    }
    (void)bus_->dispatch_all();
    const std::string response = std::string{"{\"ok\":true,\"command\":\""} + command +
        "\",\"armState\":\"" + arm_state_name(target) + "\"}";
    return send_json(request,response.c_str(),response.size());
}

void SystemHttp::remember_client(int socket_fd) {
    if (socket_fd < 0) return;
    for (const int client : clients_) if (client == socket_fd) return;
    for (auto& client : clients_) {
        if (client < 0) {
            client = socket_fd;
            return;
        }
    }
    clients_[0] = socket_fd;
}

esp_err_t SystemHttp::websocket(httpd_req_t* request) {
    auto* self = self_from(request);
    if (!self) return ESP_FAIL;
    if (!self->authenticated_request(request)) return request_auth::send_login_required(request);
    self->remember_client(httpd_req_to_sockfd(request));
    return ESP_OK;
}

void SystemHttp::on_event(const hg::SystemEvent& event,void* context) {
    auto* self = static_cast<SystemHttp*>(context);
    if (!self) return;
    self->record(event);
    self->broadcast(event);
}

void SystemHttp::record(const hg::SystemEvent& event) {
    event_log_.append(event.timestamp_ms,severity_for(event.type),static_cast<std::uint16_t>(event.type),hg::system_event_type_name(event.type));
}

std::string SystemHttp::events_json() const {
    std::ostringstream out;
    out << "{\"capacity\":" << hg::EventLog::capacity << ",\"events\":[";
    for (std::size_t i = 0; i < event_log_.size(); ++i) {
        if (i) out << ',';
        const auto& item = event_log_.at_oldest(i);
        out << "{\"sequence\":" << item.sequence << ",\"timestampMs\":" << item.timestamp_ms
            << ",\"severity\":\"" << severity_name(item.severity) << "\",\"code\":" << item.code
            << ",\"event\":\"" << item.text.data() << "\"}";
    }
    out << "]}";
    return out.str();
}

void SystemHttp::broadcast(const hg::SystemEvent& event) {
    if (!server_) return;
    const std::string payload = hg::system_event_json(event);
    httpd_ws_frame_t frame{};
    frame.type = HTTPD_WS_TYPE_TEXT;
    frame.payload = reinterpret_cast<std::uint8_t*>(const_cast<char*>(payload.data()));
    frame.len = payload.size();
    for (auto& client : clients_) {
        if (client < 0) continue;
        if (httpd_ws_send_frame_async(server_,client,&frame) != ESP_OK) client = -1;
    }
}

}  // namespace homeguard::idf