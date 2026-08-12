#include "hg_cloud_http.hpp"

#include "hg_cloud_link.hpp"
#include "nvs.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>

namespace homeguard::idf {
namespace {

bool parse_json_string(const std::string& body, const char* key, std::string& value)
{
    const std::string marker = std::string{"\""} + key + "\"";
    auto pos = body.find(marker);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + marker.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
    if (pos >= body.size() || body[pos] != '"') return false;
    ++pos;
    value.clear();
    bool escaped = false;
    for (; pos < body.size(); ++pos) {
        const char ch = body[pos];
        if (escaped) {
            if (ch == '"' || ch == '\\' || ch == '/') value.push_back(ch);
            else if (ch == 'n') value.push_back('\n');
            else if (ch == 'r') value.push_back('\r');
            else if (ch == 't') value.push_back('\t');
            else return false;
            escaped = false;
            continue;
        }
        if (ch == '\\') { escaped = true; continue; }
        if (ch == '"') return true;
        value.push_back(ch);
    }
    return false;
}

template <std::size_t N>
bool copy_text(std::array<char, N>& destination, const std::string& source)
{
    if (source.size() >= N) return false;
    destination.fill('\0');
    std::copy(source.begin(), source.end(), destination.begin());
    return true;
}

esp_err_t send_json(httpd_req_t* request, const char* body)
{
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, body, static_cast<ssize_t>(std::strlen(body)));
}

}  // namespace

esp_err_t CloudHttp::initialize(CloudLink* cloud, homeguard::AccessControl* access)
{
    if (cloud == nullptr || access == nullptr) return ESP_ERR_INVALID_ARG;
    cloud_ = cloud;
    access_ = access;

    CloudConfigRecord restored{};
    const auto error = store_.load(restored);
    if (error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (error != ESP_OK) return error;

    config_ = restored;
    return apply(config_, false);
}

esp_err_t CloudHttp::register_handlers(httpd_handle_t server)
{
    if (server == nullptr || cloud_ == nullptr || access_ == nullptr) return ESP_ERR_INVALID_ARG;
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
    if (request == nullptr) return ESP_ERR_INVALID_ARG;
    auto* self = static_cast<CloudHttp*>(request->user_ctx);
    if (self == nullptr || self->cloud_ == nullptr) return ESP_ERR_INVALID_STATE;

    char body[320]{};
    std::snprintf(body, sizeof(body),
                  "{\"ok\":true,\"configured\":%s,\"connected\":%s,\"enabled\":%s,\"device_id\":\"%s\",\"connect_count\":%u,\"disconnect_count\":%u}",
                  self->cloud_->configured() ? "true" : "false",
                  self->cloud_->connected() ? "true" : "false",
                  self->config_.enabled ? "true" : "false",
                  self->cloud_->device_id(),
                  static_cast<unsigned>(self->cloud_->connect_count()),
                  static_cast<unsigned>(self->cloud_->disconnect_count()));
    return send_json(request, body);
}

esp_err_t CloudHttp::config_post(httpd_req_t* request)
{
    auto* self = request == nullptr ? nullptr : static_cast<CloudHttp*>(request->user_ctx);
    return self == nullptr ? ESP_FAIL : self->handle_config(request);
}

esp_err_t CloudHttp::handle_config(httpd_req_t* request)
{
    if (request->content_len == 0 || request->content_len > 768) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }

    std::string body(request->content_len, '\0');
    const auto received = httpd_req_recv(request, body.data(), body.size());
    if (received <= 0) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"read_failed\"}");
    }
    body.resize(static_cast<std::size_t>(received));

    std::string actor;
    std::string credential;
    if (!parse_json_string(body, "actor", actor) || !parse_json_string(body, "credential", credential)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return send_json(request, "{\"ok\":false,\"reason\":\"admin_credential_required\"}");
    }
    const auto decision = access_->authorize(actor, credential, "cloud.configure");
    std::fill(credential.begin(), credential.end(), '\0');
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return send_json(request, "{\"ok\":false,\"reason\":\"access_denied\"}");
    }

    std::string broker;
    std::string username;
    std::string password;
    std::string enabled;
    if (!parse_json_string(body, "enabled", enabled)) enabled = "true";

    CloudConfigRecord candidate{};
    candidate.enabled = enabled != "false" && enabled != "0";
    if (candidate.enabled) {
        if (!parse_json_string(body, "broker_uri", broker) || broker.empty() ||
            !parse_json_string(body, "username", username) ||
            !parse_json_string(body, "password", password) ||
            !copy_text(candidate.broker_uri, broker) || !copy_text(candidate.username, username) || !copy_text(candidate.password, password)) {
            std::fill(password.begin(), password.end(), '\0');
            httpd_resp_set_status(request, "400 Bad Request");
            return send_json(request, "{\"ok\":false,\"reason\":\"invalid_cloud_config\"}");
        }
        if (broker.rfind("mqtts://", 0) != 0 && broker.rfind("mqtt://", 0) != 0) {
            std::fill(password.begin(), password.end(), '\0');
            httpd_resp_set_status(request, "400 Bad Request");
            return send_json(request, "{\"ok\":false,\"reason\":\"invalid_broker_uri\"}");
        }
    }

    const auto error = apply(candidate, true);
    std::fill(password.begin(), password.end(), '\0');
    if (error != ESP_OK) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return send_json(request, "{\"ok\":false,\"reason\":\"cloud_apply_failed\"}");
    }
    return send_json(request, "{\"ok\":true,\"state\":\"applied\"}");
}

esp_err_t CloudHttp::apply(const CloudConfigRecord& config, bool persist)
{
    if (cloud_ == nullptr) return ESP_ERR_INVALID_STATE;
    cloud_->stop();

    if (persist) {
        const auto save_error = config.enabled ? store_.save(config) : store_.clear();
        if (save_error != ESP_OK) return save_error;
    }

    config_ = config;
    if (!config_.enabled) return ESP_OK;
    return cloud_->start(config_.broker_uri.data(), config_.username.data(), config_.password.data());
}

}  // namespace homeguard::idf
