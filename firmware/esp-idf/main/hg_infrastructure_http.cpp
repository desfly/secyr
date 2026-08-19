#include "hg_infrastructure_http.hpp"
#include "hg_hardware_bootstrap.hpp"
#include "homeguard/hardware_runtime.hpp"

#include <cstddef>

namespace homeguard::idf {

esp_err_t InfrastructureHttp::register_handlers(
    httpd_handle_t server,
    HardwareBootstrap* hardware)
{
    if (server == nullptr || hardware == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    // Hardware status is read-only. The former POST /api/v1/diagnostics/rgb-test
    // route deliberately is not registered: it was unauthenticated and executed
    // a 3-second blocking LED diagnostic inside the HTTP server task, allowing
    // any LAN client to repeatedly stall the control/UI server. RGB diagnostics
    // remain available internally for controlled boot/reset paths.
    const httpd_uri_t status_route{
        .uri = "/api/v1/hardware/status",
        .method = HTTP_GET,
        .handler = &InfrastructureHttp::status_get,
        .user_ctx = hardware,
    };
    return httpd_register_uri_handler(server, &status_route);
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

esp_err_t InfrastructureHttp::rgb_test_post(httpd_req_t* request)
{
    // Kept only to preserve the class ABI/source shape for now. This handler is
    // intentionally unreachable because register_handlers() does not register
    // a remote RGB-test URI. Never re-expose it without authenticated,
    // non-blocking diagnostics authorization and a regression gate.
    if (request == nullptr) return ESP_ERR_INVALID_ARG;
    httpd_resp_set_status(request, "404 Not Found");
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(
        request,
        "{\"ok\":false,\"reason\":\"remote_rgb_test_disabled\"}",
        HTTPD_RESP_USE_STRLEN);
}

}  // namespace homeguard::idf
