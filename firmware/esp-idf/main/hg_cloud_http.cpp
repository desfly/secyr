#include "hg_cloud_http.hpp"

#include "hg_cloud_link.hpp"
#include "hg_cloud_nvs.hpp"
#include "hg_http_util.hpp"
#include "hg_request_auth.hpp"
#include "homeguard/access_control.hpp"
#include "nvs.h"

#include <algorithm>
#include <cstddef>
#include <string>

namespace homeguard::idf {
namespace {

bool parse_json_bool(const std::string& body, const char* key, bool& value)
{
    const auto pos = http_util::value_offset(body, key);
    if (pos == std::string::npos) return false;
    if (body.compare(pos, 4, "true") == 0) { value = true; return true; }
    if (body.compare(pos, 5, "false") == 0) { value = false; return true; }
    return false;
}

void scrub_cloud_password(CloudConfig& config)
{
    http_util::scrub(config.password);
}

esp_err_t send_json(httpd_req_t* request, const std::string& body)
{
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

}  // namespace

esp_err_t CloudHttp::register_handlers(
    httpd_handle_t server,
    CloudLink* cloud,
    CloudNvsStore* store,
    homeguard::AccessControl* access_control)
{
    if (server == nullptr || cloud == nullptr || store == nullptr || access_control == nullptr) return ESP_ERR_INVALID_ARG;
    cloud_ = cloud;
    store_ = store;
    access_control_ = access_control;
    const httpd_uri_t routes[] = {
        {.uri = "/api/v1/cloud/status", .method = HTTP_GET, .handler = &CloudHttp::status_get, .user_ctx = this},
        {.uri = "/api/v1/cloud/config", .method = HTTP_POST, .handler = &CloudHttp::config_post, .user_ctx = this},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

esp_err_t CloudHttp::status_get(httpd_req_t* request)
{
    if (request == nullptr || request->user_ctx == nullptr) return ESP_ERR_INVALID_ARG;
    auto* self = static_cast<CloudHttp*>(request->user_ctx);
    if (self->cloud_ == nullptr || self->access_control_ == nullptr) return ESP_FAIL;
    if (!request_auth::authenticated(request, *self->access_control_)) {
        return request_auth::send_login_required(request);
    }

    const bool configured = self->cloud_->configured();
    const bool connected = self->cloud_->connected();
    const char* state = connected ? "connected" : (configured ? "connecting" : "disabled");
    const std::string body =
        std::string{"{\"ok\":true,\"state\":\""} + state +
        "\",\"configured\":" + (configured ? "true" : "false") +
        ",\"connected\":" + (connected ? "true" : "false") +
        ",\"deviceId\":\"" + self->cloud_->device_id() +
        "\",\"connectCount\":" + std::to_string(self->cloud_->connect_count()) +
        ",\"disconnectCount\":" + std::to_string(self->cloud_->disconnect_count()) + "}";
    return send_json(request, body);
}

esp_err_t CloudHttp::config_post(httpd_req_t* request)
{
    if (request == nullptr || request->user_ctx == nullptr) return ESP_ERR_INVALID_ARG;
    return static_cast<CloudHttp*>(request->user_ctx)->handle_config(request);
}

esp_err_t CloudHttp::handle_config(httpd_req_t* request)
{
    std::string body;
    if (!http_util::read_body(request, 1024U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }

    std::string actor;
    if (!http_util::parse_json_string(body, "actor", actor) || actor.empty()) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "401 Unauthorized");
        return send_json(request, "{\"ok\":false,\"reason\":\"session_actor_required\"}");
    }
    if (!request_auth::authenticated_actor(request, *access_control_, actor)) {
        http_util::scrub(body);
        return request_auth::send_login_required(request);
    }
    const auto decision = access_control_->authorize_session(actor, "cloud.configure");
    if (decision != homeguard::AuditDecision::Allowed) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "403 Forbidden");
        return send_json(request, "{\"ok\":false,\"reason\":\"forbidden\"}");
    }

    CloudConfig config{};
    if (!parse_json_bool(body, "enabled", config.enabled)) config.enabled = true;
    (void)http_util::parse_json_string(body, "brokerUri", config.broker_uri);
    (void)http_util::parse_json_string(body, "username", config.username);
    (void)http_util::parse_json_string(body, "password", config.password);
    http_util::scrub(body);
    if (config.enabled && (config.broker_uri.empty() || config.broker_uri.size() > 256 ||
                           config.username.size() > 128 || config.password.size() > 128)) {
        scrub_cloud_password(config);
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_cloud_config\"}");
    }

    CloudConfig previous{};
    const auto previous_error = store_->load(previous);
    const bool had_previous = previous_error == ESP_OK;
    if (previous_error != ESP_OK && previous_error != ESP_ERR_NVS_NOT_FOUND) {
        scrub_cloud_password(config);
        httpd_resp_set_status(request, "500 Internal Server Error");
        return send_json(request, "{\"ok\":false,\"reason\":\"previous_config_unreadable\"}");
    }

    const auto save_error = store_->save(config);
    if (save_error != ESP_OK) {
        scrub_cloud_password(config);
        scrub_cloud_password(previous);
        httpd_resp_set_status(request, "500 Internal Server Error");
        return send_json(request, "{\"ok\":false,\"reason\":\"persist_failed\"}");
    }

    cloud_->stop();
    esp_err_t start_error = ESP_OK;
    if (config.enabled) {
        start_error = cloud_->start(config.broker_uri.c_str(), config.username.c_str(), config.password.c_str());
    }

    if (start_error != ESP_OK) {
        const auto rollback_error = had_previous ? store_->save(previous) : store_->clear();
        esp_err_t restore_runtime_error = ESP_OK;
        if (rollback_error == ESP_OK && had_previous && previous.enabled) {
            restore_runtime_error = cloud_->start(
                previous.broker_uri.c_str(), previous.username.c_str(), previous.password.c_str());
        }
        scrub_cloud_password(config);
        scrub_cloud_password(previous);
        httpd_resp_set_status(request, "503 Service Unavailable");
        if (rollback_error != ESP_OK || restore_runtime_error != ESP_OK) {
            return send_json(request, "{\"ok\":false,\"reason\":\"mqtt_start_failed_rollback_failed\"}");
        }
        return send_json(request, "{\"ok\":false,\"reason\":\"mqtt_start_failed\",\"rolledBack\":true}");
    }

    scrub_cloud_password(config);
    scrub_cloud_password(previous);
    return send_json(request, config.enabled ? "{\"ok\":true,\"state\":\"connecting\"}" : "{\"ok\":true,\"state\":\"disabled\"}");
}

}  // namespace homeguard::idf
