#include "hg_build_http.hpp"
#include "hg_build_info.hpp"
#include "hg_request_auth.hpp"

namespace homeguard::idf {

esp_err_t BuildHttp::register_handlers(httpd_handle_t server, homeguard::AccessControl* access_control)
{
    if (server == nullptr || access_control == nullptr) return ESP_ERR_INVALID_ARG;
    access_control_ = access_control;

    const httpd_uri_t route{
        .uri = "/api/v1/build",
        .method = HTTP_GET,
        .handler = &BuildHttp::build_get,
        .user_ctx = this,
    };

    return httpd_register_uri_handler(server, &route);
}

esp_err_t BuildHttp::build_get(httpd_req_t* request)
{
    if (request == nullptr || request->user_ctx == nullptr) return ESP_ERR_INVALID_ARG;
    auto* self = static_cast<BuildHttp*>(request->user_ctx);
    if (self->access_control_ == nullptr || !request_auth::authenticated(request, *self->access_control_)) {
        return request_auth::send_login_required(request);
    }

    const auto body = build_info_json(current_build_info());
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

}  // namespace homeguard::idf
