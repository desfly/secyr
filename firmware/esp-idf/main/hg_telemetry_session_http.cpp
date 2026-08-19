#include "hg_telemetry_session_http.hpp"
#include "hg_http_util.hpp"
#include "hg_request_auth.hpp"

#include <string>

namespace homeguard::idf {
namespace {

TelemetrySessionHttp* self_from(httpd_req_t* request) {
    return static_cast<TelemetrySessionHttp*>(request->user_ctx);
}

esp_err_t send_json(httpd_req_t* request, const std::string& body) {
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

}  // namespace

esp_err_t TelemetrySessionHttp::register_handlers(httpd_handle_t server,
                                                   AccessControl* access,
                                                   WebsocketTelemetry* telemetry) {
    if (server == nullptr || access == nullptr || telemetry == nullptr) return ESP_ERR_INVALID_ARG;
    access_ = access;
    telemetry_ = telemetry;
    const httpd_uri_t route{
        .uri = "/api/v1/telemetry/session",
        .method = HTTP_POST,
        .handler = &TelemetrySessionHttp::login_post,
        .user_ctx = this,
    };
    return httpd_register_uri_handler(server, &route);
}

esp_err_t TelemetrySessionHttp::login_post(httpd_req_t* request) {
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_login(request);
}

esp_err_t TelemetrySessionHttp::handle_login(httpd_req_t* request) {
    if (access_ == nullptr || telemetry_ == nullptr) return ESP_FAIL;

    std::string body;
    if (!http_util::read_body(request, 256U, body)) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }

    std::string actor;
    if (!http_util::parse_json_string(body, "actor", actor) || actor.empty() || actor.size() > 23U) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "401 Unauthorized");
        return send_json(request, "{\"ok\":false,\"reason\":\"session_actor_required\"}");
    }
    http_util::scrub(body);

    if (!request_auth::authenticated_actor(request, *access_, actor)) {
        return request_auth::send_login_required(request);
    }

    std::string token = telemetry_->issue_session_token();
    if (token.size() < 32U) {
        http_util::scrub(token);
        httpd_resp_set_status(request, "503 Service Unavailable");
        return send_json(request, "{\"ok\":false,\"reason\":\"telemetry_unavailable\"}");
    }

    const std::string response = std::string{"{\"ok\":true,\"telemetryToken\":\""} + token + "\"}";
    const auto result = send_json(request, response);
    http_util::scrub(token);
    return result;
}

}  // namespace homeguard::idf
