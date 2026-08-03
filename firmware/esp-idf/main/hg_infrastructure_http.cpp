#include "hg_infrastructure_http.hpp"
#include "hg_hardware_bootstrap.hpp"
#include "homeguard/hardware_runtime.hpp"

namespace homeguard::idf {

esp_err_t InfrastructureHttp::register_handlers(
    httpd_handle_t server,
    HardwareBootstrap* hardware)
{
    if (server == nullptr || hardware == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    const httpd_uri_t route{
        .uri = "/api/v1/hardware/status",
        .method = HTTP_GET,
        .handler = &InfrastructureHttp::status_get,
        .user_ctx = hardware,
    };

    return httpd_register_uri_handler(
        server,
        &route);
}

esp_err_t InfrastructureHttp::status_get(
    httpd_req_t* request)
{
    auto* hardware =
        static_cast<HardwareBootstrap*>(
            request->user_ctx);

    if (hardware == nullptr) {
        return httpd_resp_send_err(
            request,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "hardware unavailable");
    }

    const auto body =
        hardware_runtime_json(
            hardware->status());

    httpd_resp_set_type(
        request,
        "application/json");
    return httpd_resp_send(
        request,
        body.c_str(),
        body.size());
}

}  // namespace homeguard::idf
