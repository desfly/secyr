#include "hg_inputs_http.hpp"
#include "hg_board_hw678.hpp"

#include "driver/gpio.h"

#include <sstream>

namespace homeguard::idf {

esp_err_t InputsHttp::register_handlers(httpd_handle_t server)
{
    if (server == nullptr) return ESP_ERR_INVALID_ARG;

    gpio_config_t config{};
    config.pin_bit_mask =
        (1ULL << static_cast<unsigned>(homeguard::board::kTamper)) |
        (1ULL << static_cast<unsigned>(homeguard::board::kPowerFail));
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    const auto gpio_error = gpio_config(&config);
    if (gpio_error != ESP_OK) return gpio_error;

    const httpd_uri_t route{
        .uri = "/api/v1/system/inputs",
        .method = HTTP_GET,
        .handler = &InputsHttp::inputs_get,
        .user_ctx = this,
    };
    return httpd_register_uri_handler(server, &route);
}

esp_err_t InputsHttp::inputs_get(httpd_req_t* request)
{
    const int tamper = gpio_get_level(homeguard::board::kTamper);
    const int power_fail = gpio_get_level(homeguard::board::kPowerFail);

    std::ostringstream out;
    out << "{\"inputs\":["
        << "{\"index\":0,\"name\":\"Tamper\",\"gpio\":"
        << static_cast<int>(homeguard::board::kTamper)
        << ",\"active\":" << (tamper != 0 ? "true" : "false")
        << ",\"state\":\"" << (tamper != 0 ? "high" : "low") << "\"},"
        << "{\"index\":1,\"name\":\"Power Fail\",\"gpio\":"
        << static_cast<int>(homeguard::board::kPowerFail)
        << ",\"active\":" << (power_fail != 0 ? "true" : "false")
        << ",\"state\":\"" << (power_fail != 0 ? "high" : "low") << "\"}]}";

    const auto body = out.str();
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, body.c_str(), body.size());
}

}  // namespace homeguard::idf
