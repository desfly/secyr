#include "hg_infrastructure_http.hpp"
#include "hg_hardware_bootstrap.hpp"
#include "hg_rgb_diagnostic.hpp"
#include "homeguard/hardware_runtime.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace homeguard::idf {

esp_err_t InfrastructureHttp::register_handlers(
    httpd_handle_t server,
    HardwareBootstrap* hardware)
{
    if (server == nullptr || hardware == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    const httpd_uri_t routes[] = {
        {
            .uri = "/api/v1/hardware/status",
            .method = HTTP_GET,
            .handler = &InfrastructureHttp::status_get,
            .user_ctx = hardware,
        },
        {
            .uri = "/api/v1/diagnostics/rgb-test",
            .method = HTTP_POST,
            .handler = &InfrastructureHttp::rgb_test_post,
            .user_ctx = hardware,
        },
    };

    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
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
    if (request == nullptr) return ESP_ERR_INVALID_ARG;

    char query[64]{};
    char gpio_text[8]{};
    if (httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "gpio", gpio_text, sizeof(gpio_text)) != ESP_OK) {
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(
            request,
            "{\"ok\":false,\"reason\":\"gpio_required\",\"allowed\":[38,48]}",
            HTTPD_RESP_USE_STRLEN);
    }

    const int gpio = std::atoi(gpio_text);
    if (gpio != 38 && gpio != 48) {
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_send(
            request,
            "{\"ok\":false,\"reason\":\"unsupported_gpio\",\"allowed\":[38,48]}",
            HTTPD_RESP_USE_STRLEN);
    }

    const auto error = RgbDiagnostic::test_white(gpio, 3000U);
    if (error != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return httpd_resp_send(
            request,
            "{\"ok\":false,\"reason\":\"rgb_driver_failed\"}",
            HTTPD_RESP_USE_STRLEN);
    }

    char response[80]{};
    const auto written = std::snprintf(
        response,
        sizeof(response),
        "{\"ok\":true,\"gpio\":%d,\"color\":\"white\",\"durationMs\":3000}",
        gpio);
    if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(response)) {
        return ESP_FAIL;
    }

    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, response, written);
}

}  // namespace homeguard::idf
