#include "hg_relay_http.hpp"

#include "hg_http_util.hpp"
#include "hg_relay_runtime.hpp"
#include "hg_request_auth.hpp"
#include "homeguard/access_control.hpp"

#include <string>

namespace homeguard::idf {
namespace {

bool parse_bool(const std::string& body, const char* key, bool& value)
{
    const auto pos = http_util::value_offset(body, key);
    if (pos == std::string::npos) return false;
    if (body.compare(pos, 4, "true") == 0) { value = true; return true; }
    if (body.compare(pos, 5, "false") == 0) { value = false; return true; }
    return false;
}

esp_err_t send_state(httpd_req_t* request, RelayRuntime& relays)
{
    const auto state = relays.state();
    const std::string body = std::string{"{\"ok\":true,\"lightActive\":"} +
        (state.light_active ? "true" : "false") +
        ",\"lightManual\":" + (state.light_manual ? "true" : "false") +
        ",\"lightAutomatic\":" + (state.light_automatic ? "true" : "false") +
        ",\"lockActive\":" + (state.lock_active ? "true" : "false") +
        ",\"lockRemainingMs\":" + std::to_string(state.lock_remaining_ms) +
        ",\"valve1Active\":" + (state.valve1_active ? "true" : "false") +
        ",\"valve2Active\":" + (state.valve2_active ? "true" : "false") + "}";
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

bool authorize_actor(httpd_req_t* request, homeguard::AccessControl* access, const std::string& actor)
{
    return access != nullptr && request_auth::authenticated_actor(request, *access, actor);
}

}  // namespace

esp_err_t RelayHttp::register_handlers(
    httpd_handle_t server,
    RelayRuntime* relays,
    homeguard::AccessControl* access_control)
{
    if (server == nullptr || relays == nullptr || access_control == nullptr) return ESP_ERR_INVALID_ARG;
    relays_ = relays;
    access_control_ = access_control;

    const httpd_uri_t routes[] = {
        {.uri="/api/v1/outputs/relay-state", .method=HTTP_GET, .handler=&RelayHttp::state_get, .user_ctx=this},
        {.uri="/api/v1/outputs/light", .method=HTTP_POST, .handler=&RelayHttp::light_post, .user_ctx=this},
        {.uri="/api/v1/outputs/lock/pulse", .method=HTTP_POST, .handler=&RelayHttp::lock_post, .user_ctx=this},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

esp_err_t RelayHttp::state_get(httpd_req_t* request)
{
    auto* self = request == nullptr ? nullptr : static_cast<RelayHttp*>(request->user_ctx);
    if (self == nullptr || self->relays_ == nullptr || self->access_control_ == nullptr) return ESP_FAIL;
    if (!request_auth::authenticated(request, *self->access_control_)) return request_auth::send_login_required(request);
    return send_state(request, *self->relays_);
}

esp_err_t RelayHttp::light_post(httpd_req_t* request)
{
    auto* self = request == nullptr ? nullptr : static_cast<RelayHttp*>(request->user_ctx);
    if (self == nullptr || self->relays_ == nullptr || self->access_control_ == nullptr) return ESP_FAIL;

    std::string body;
    std::string actor;
    bool active{};
    if (!http_util::read_body(request, 256U, body) ||
        !http_util::parse_json_string(body, "actor", actor) || actor.empty() ||
        !parse_bool(body, "active", active)) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"invalid_command\"}", -1);
    }

    if (!authorize_actor(request, self->access_control_, actor)) {
        http_util::scrub(body);
        return request_auth::send_login_required(request);
    }
    const auto decision = self->access_control_->authorize_session(actor, "output.control");
    http_util::scrub(body);
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"forbidden\"}", -1);
    }
    if (!self->relays_->set_manual_light(active)) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"relay_unavailable\"}", -1);
    }
    return send_state(request, *self->relays_);
}

esp_err_t RelayHttp::lock_post(httpd_req_t* request)
{
    auto* self = request == nullptr ? nullptr : static_cast<RelayHttp*>(request->user_ctx);
    if (self == nullptr || self->relays_ == nullptr || self->access_control_ == nullptr) return ESP_FAIL;

    std::string body;
    if (!http_util::read_body(request, 256U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"invalid_body\"}", -1);
    }

    std::string actor;
    if (!http_util::parse_json_string(body, "actor", actor) || actor.empty()) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"missing_actor\"}", -1);
    }

    if (!authorize_actor(request, self->access_control_, actor)) {
        http_util::scrub(body);
        return request_auth::send_login_required(request);
    }

    const auto decision = self->access_control_->authorize_session(actor, "output.control");
    http_util::scrub(body);
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"forbidden\"}", -1);
    }

    if (!self->relays_->request_lock_pulse()) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"relay_unavailable\"}", -1);
    }
    return send_state(request, *self->relays_);
}

}  // namespace homeguard::idf
