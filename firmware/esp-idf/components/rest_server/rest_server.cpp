#include "rest_server.hpp"
#include "cJSON.h"
#include "esp_https_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <string>

namespace {
constexpr char tag[] = "hg_rest";
constexpr size_t max_body = 1024;

uint64_t now_ms() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }

int send_json(httpd_req_t* request, const char* status, const std::string& body) {
    httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

bool read_json(httpd_req_t* request, std::array<char, max_body + 1>& body, cJSON*& root) {
    if (request->content_len <= 0 || static_cast<size_t>(request->content_len) > max_body) return false;
    size_t offset = 0;
    while (offset < static_cast<size_t>(request->content_len)) {
        const int count = httpd_req_recv(request, body.data() + offset,
                                         request->content_len - static_cast<int>(offset));
        if (count <= 0) return false;
        offset += static_cast<size_t>(count);
    }
    body[offset] = '\0';
    root = cJSON_ParseWithLength(body.data(), offset);
    return root != nullptr;
}

std::string json_string(cJSON* root, const char* key) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : "";
}

bool parse_u64_text(std::string_view text, uint64_t& value) {
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

bool json_u64(cJSON* root, const char* key, uint64_t& value) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsString(item) && item->valuestring) return parse_u64_text(item->valuestring, value);
    if (cJSON_IsNumber(item) && item->valuedouble >= 0.0 &&
        item->valuedouble <= static_cast<double>(std::numeric_limits<uint64_t>::max())) {
        value = static_cast<uint64_t>(item->valuedouble);
        return true;
    }
    return false;
}

bool json_u32(cJSON* root, const char* key, uint32_t& value) {
    uint64_t temporary = 0;
    if (!json_u64(root, key, temporary) || temporary > std::numeric_limits<uint32_t>::max()) return false;
    value = static_cast<uint32_t>(temporary);
    return true;
}
}

bool RestServer::begin(hg::Controller& controller, std::string_view local_api_token,
                       std::string_view certificate_pem, std::string_view private_key_pem,
                       std::string_view device_id, uint16_t port) {
    if (server_ || port == 0U || local_api_token.size() < 32U || certificate_pem.empty() ||
        private_key_pem.empty() || device_id.empty()) return false;
    controller_ = &controller;
    token_.reset(local_api_token);
    certificate_pem_ = certificate_pem;
    private_key_pem_ = private_key_pem;
    device_id_ = device_id;

    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
    config.httpd.server_port = port;
    config.httpd.max_uri_handlers = 8;
    config.servercert = reinterpret_cast<const uint8_t*>(certificate_pem_.c_str());
    config.servercert_len = certificate_pem_.size() + 1U;
    config.prvtkey_pem = reinterpret_cast<const uint8_t*>(private_key_pem_.c_str());
    config.prvtkey_len = private_key_pem_.size() + 1U;
    httpd_handle_t handle = nullptr;
    if (httpd_ssl_start(&handle, &config) != ESP_OK) {
        stop();
        return false;
    }
    server_ = handle;

    httpd_uri_t status_uri{};
    status_uri.uri = "/api/status"; status_uri.method = HTTP_GET;
    status_uri.handler = &RestServer::status_entry; status_uri.user_ctx = this;
    httpd_uri_t health_uri{};
    health_uri.uri = "/api/health"; health_uri.method = HTTP_GET;
    health_uri.handler = &RestServer::health_entry; health_uri.user_ctx = this;
    httpd_uri_t challenge_uri{};
    challenge_uri.uri = "/api/challenge"; challenge_uri.method = HTTP_POST;
    challenge_uri.handler = &RestServer::challenge_entry; challenge_uri.user_ctx = this;
    httpd_uri_t command_uri{};
    command_uri.uri = "/api/command"; command_uri.method = HTTP_POST;
    command_uri.handler = &RestServer::command_entry; command_uri.user_ctx = this;

    if (httpd_register_uri_handler(handle, &status_uri) != ESP_OK ||
        httpd_register_uri_handler(handle, &health_uri) != ESP_OK ||
        httpd_register_uri_handler(handle, &challenge_uri) != ESP_OK ||
        httpd_register_uri_handler(handle, &command_uri) != ESP_OK) {
        stop();
        return false;
    }
    ESP_LOGI(tag, "authenticated HTTPS API started for %s", device_id_.c_str());
    return true;
}

void RestServer::stop() {
    if (server_) httpd_ssl_stop(static_cast<httpd_handle_t>(server_));
    server_ = nullptr;
    controller_ = nullptr;
    token_.clear();
    std::fill(private_key_pem_.begin(), private_key_pem_.end(), '\0');
    certificate_pem_.clear(); private_key_pem_.clear(); device_id_.clear();
}

bool RestServer::authorize(httpd_req_t* request) const {
    const size_t length = httpd_req_get_hdr_value_len(request, "Authorization");
    if (length == 0U || length > 320U) return false;
    std::array<char, 321> value{};
    if (httpd_req_get_hdr_value_str(request, "Authorization", value.data(), length + 1U) != ESP_OK) return false;
    return token_.authorized(std::string_view(value.data(), length));
}

int RestServer::status_entry(httpd_req_t* request) { return static_cast<RestServer*>(request->user_ctx)->status(request); }
int RestServer::health_entry(httpd_req_t* request) { return static_cast<RestServer*>(request->user_ctx)->health(request); }
int RestServer::challenge_entry(httpd_req_t* request) { return static_cast<RestServer*>(request->user_ctx)->challenge(request); }
int RestServer::command_entry(httpd_req_t* request) { return static_cast<RestServer*>(request->user_ctx)->command(request); }

int RestServer::status(httpd_req_t* request) {
    if (!authorize(request)) {
        httpd_resp_set_hdr(request, "WWW-Authenticate", "Bearer realm=\"homeguard-local\"");
        return send_json(request, "401 Unauthorized", R"({"error":"unauthorized"})");
    }
    return send_json(request, "200 OK", hg::telemetry_json(controller_->telemetry(now_ms(), 0)));
}

int RestServer::health(httpd_req_t* request) {
    if (!authorize(request)) return send_json(request, "401 Unauthorized", R"({"error":"unauthorized"})");
    return send_json(request, "200 OK", hg::health_json(controller_->health(), controller_->transport()));
}

int RestServer::challenge(httpd_req_t* request) {
    if (!authorize(request)) return send_json(request, "401 Unauthorized", R"({"error":"unauthorized"})");
    std::array<char, max_body + 1> body{}; cJSON* root = nullptr;
    if (!read_json(request, body, root)) return send_json(request, "400 Bad Request", R"({"error":"invalid_json"})");
    const auto command = hg::parse_command_type(json_string(root, "command"));
    cJSON_Delete(root); std::fill(body.begin(), body.end(), '\0');
    if (!command || !hg::dangerous(*command)) return send_json(request, "422 Unprocessable Entity", R"({"error":"challenge_not_required"})");
    return send_json(request, "200 OK", hg::challenge_json(controller_->issue_challenge(*command, now_ms())));
}

int RestServer::command(httpd_req_t* request) {
    if (!authorize(request)) return send_json(request, "401 Unauthorized", R"({"error":"unauthorized"})");
    std::array<char, max_body + 1> body{}; cJSON* root = nullptr;
    if (!read_json(request, body, root)) return send_json(request, "400 Bad Request", R"({"error":"invalid_json"})");
    uint64_t request_id = 0; uint32_t challenge_token = 0;
    const auto command_type = hg::parse_command_type(json_string(root, "command"));
    const bool request_ok = json_u64(root, "requestId", request_id);
    const bool challenge_present = json_u32(root, "challenge", challenge_token);
    cJSON_Delete(root); std::fill(body.begin(), body.end(), '\0');
    if (!request_ok || !command_type) return send_json(request, "422 Unprocessable Entity", R"({"error":"invalid_command"})");
    if (hg::dangerous(*command_type) && !challenge_present) {
        return send_json(request, "409 Conflict", R"({"accepted":false,"duplicate":false,"code":"challenge_required"})");
    }
    const uint64_t received_at = now_ms();
    const hg::Command command_value{request_id, received_at, *command_type, challenge_token, true};
    const auto result = controller_->execute(command_value, received_at);
    const char* status_code = result.executed || result.duplicate ? "200 OK" : "409 Conflict";
    return send_json(request, status_code, hg::command_result_json(result));
}
