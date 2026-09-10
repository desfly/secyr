#include "hg_zone_http.hpp"

#include "hg_zone_monitor.hpp"
#include "hg_http_util.hpp"
#include "hg_request_auth.hpp"
#include "homeguard/access_control.hpp"

#include <cstddef>
#include <cstdlib>
#include <string>

namespace homeguard::idf {
namespace {
ZoneHttp* self_from(httpd_req_t* request)
{
    return request == nullptr ? nullptr : static_cast<ZoneHttp*>(request->user_ctx);
}

esp_err_t send_json(httpd_req_t* request, const std::string& body)
{
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}
}

esp_err_t ZoneHttp::register_handlers(
    httpd_handle_t server,
    ZoneMonitor* monitor,
    homeguard::AccessControl* access_control)
{
    if (server == nullptr || monitor == nullptr || access_control == nullptr) return ESP_ERR_INVALID_ARG;
    monitor_ = monitor;
    access_control_ = access_control;

    const httpd_uri_t routes[] = {
        {.uri="/api/v1/zones/live", .method=HTTP_GET, .handler=&ZoneHttp::live_get, .user_ctx=this},
        {.uri="/api/v1/zones/name", .method=HTTP_POST, .handler=&ZoneHttp::name_post, .user_ctx=this},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

esp_err_t ZoneHttp::live_get(httpd_req_t* request)
{
    auto* self = self_from(request);
    if (self == nullptr || self->monitor_ == nullptr || self->access_control_ == nullptr) return ESP_FAIL;
    if (!request_auth::authenticated(request, *self->access_control_)) {
        return request_auth::send_login_required(request);
    }
    return send_json(request, self->monitor_->snapshot_json());
}

esp_err_t ZoneHttp::name_post(httpd_req_t* request)
{
    auto* self = self_from(request);
    if (self == nullptr || self->monitor_ == nullptr || self->access_control_ == nullptr) return ESP_FAIL;

    std::string body;
    if (!http_util::read_body(request, 512U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"invalid_body\"}", -1);
    }

    std::string actor;
    std::string name;
    std::string id_text;
    if (!http_util::parse_json_string(body, "actor", actor) || actor.empty() ||
        !http_util::parse_json_string(body, "name", name) || name.empty() ||
        !http_util::parse_json_string(body, "id", id_text)) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"missing_fields\"}", -1);
    }

    if (!request_auth::authenticated_actor(request, *self->access_control_, actor)) {
        http_util::scrub(body);
        return request_auth::send_login_required(request);
    }

    const auto decision = self->access_control_->authorize_session(actor, "zones.configure");
    if (decision != homeguard::AuditDecision::Allowed) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "403 Forbidden");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"admin_required\"}", -1);
    }

    char* end = nullptr;
    const unsigned long id = std::strtoul(id_text.c_str(), &end, 10);
    if (end == id_text.c_str() || *end != '\0' || id < 1UL || id > ZoneMonitor::kZoneCount) {
        http_util::scrub(body);
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"invalid_zone\"}", -1);
    }

    const auto error = self->monitor_->set_name(static_cast<std::size_t>(id - 1UL), name);
    http_util::scrub(body);
    if (error != ESP_OK) {
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(request, "{\"ok\":false,\"reason\":\"name_save_failed\"}", -1);
    }

    return httpd_resp_send(request, "{\"ok\":true}", -1);
}

}  // namespace homeguard::idf
