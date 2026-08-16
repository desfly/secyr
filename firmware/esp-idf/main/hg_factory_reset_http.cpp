#include "hg_factory_reset_http.hpp"

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstddef>
#include <string>

namespace homeguard::idf {
namespace {

FactoryResetHttp* self_from(httpd_req_t* request) {
    return static_cast<FactoryResetHttp*>(request->user_ctx);
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

void reboot_task(void*) {
    vTaskDelay(pdMS_TO_TICKS(350));
    esp_restart();
}

esp_err_t send_json(httpd_req_t* request, const char* status, const std::string& body) {
    if (status != nullptr) httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

}  // namespace

esp_err_t FactoryResetHttp::register_handlers(
    httpd_handle_t server,
    homeguard::AccessControl* access_control,
    FactoryResetManager* reset_manager) {
    if (server == nullptr || access_control == nullptr || reset_manager == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    access_control_ = access_control;
    reset_manager_ = reset_manager;

    const httpd_uri_t route{
        .uri = "/api/v1/system/factory-reset",
        .method = HTTP_POST,
        .handler = &FactoryResetHttp::factory_reset_post,
        .user_ctx = this,
    };
    return httpd_register_uri_handler(server, &route);
}

esp_err_t FactoryResetHttp::factory_reset_post(httpd_req_t* request) {
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_factory_reset(request);
}

esp_err_t FactoryResetHttp::handle_factory_reset(httpd_req_t* request) {
    if (access_control_ == nullptr || reset_manager_ == nullptr) return ESP_FAIL;
    if (request->content_len == 0 || request->content_len > 512) {
        return send_json(request, "400 Bad Request", "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }

    std::string body(request->content_len, '\0');
    const auto received = httpd_req_recv(request, body.data(), body.size());
    if (received <= 0) {
        return send_json(request, "400 Bad Request", "{\"ok\":false,\"reason\":\"read_failed\"}");
    }
    body.resize(static_cast<std::size_t>(received));

    std::string actor;
    std::string credential;
    std::string confirm;
    if (!parse_json_string(body, "actor", actor) || !parse_json_string(body, "credential", credential)) {
        return send_json(request, "401 Unauthorized", "{\"ok\":false,\"reason\":\"credential_required\"}");
    }
    if (!parse_json_string(body, "confirm", confirm) || confirm != "ERASE_ALL") {
        return send_json(request, "409 Conflict", "{\"ok\":false,\"reason\":\"explicit_confirmation_required\"}");
    }

    const auto decision = access_control_->authorize(actor, credential, "system.factory_reset");
    if (decision != homeguard::AuditDecision::Allowed) {
        return send_json(
            request,
            "403 Forbidden",
            std::string{"{\"ok\":false,\"reason\":\""} + homeguard::to_string(decision) + "\"}");
    }

    const auto report = reset_manager_->erase_mutable_state();
    if (!report.ok()) {
        return send_json(
            request,
            "500 Internal Server Error",
            std::string{"{\"ok\":false,\"reason\":\"erase_failed\",\"access\":"} + std::to_string(report.access) +
                ",\"wifi\":" + std::to_string(report.wifi) +
                ",\"cloud\":" + std::to_string(report.cloud) +
                ",\"commissioning\":" + std::to_string(report.commissioning) + "}");
    }

    access_control_->clear_users();
    const auto response = send_json(
        request,
        nullptr,
        "{\"ok\":true,\"state\":\"factory_reset_complete\",\"rebooting\":true}");
    if (response == ESP_OK) {
        (void)xTaskCreate(&reboot_task, "hg_factory_reset", 2048, nullptr, 5, nullptr);
    }
    return response;
}

}  // namespace homeguard::idf
