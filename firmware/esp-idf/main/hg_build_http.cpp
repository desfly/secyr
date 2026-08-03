#include "hg_build_http.hpp"
#include "hg_build_info.hpp"

namespace homeguard::idf {

esp_err_t BuildHttp::register_handlers(httpd_handle_t server)
{
    if (server == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    const httpd_uri_t route{
        .uri = "/api/v1/build",
        .method = HTTP_GET,
        .handler = &BuildHttp::build_get,
        .user_ctx = nullptr,
    };

    return httpd_register_uri_handler(server, &route);
}

esp_err_t BuildHttp::build_get(httpd_req_t* request)
{
    const auto body = build_info_json(current_build_info());
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(
        request,
        body.c_str(),
        static_cast<ssize_t>(body.size()));
}

}  // namespace homeguard::idf
