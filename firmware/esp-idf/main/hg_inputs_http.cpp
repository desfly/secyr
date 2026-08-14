#include "hg_inputs_http.hpp"
#include "hg_board_hw678.hpp"

#include "driver/gpio.h"

#include <cstddef>
#include <sstream>
#include <string>

namespace homeguard::idf {
namespace {
InputsHttp* self_from(httpd_req_t* request)
{
    return request == nullptr ? nullptr : static_cast<InputsHttp*>(request->user_ctx);
}

const char* polarity_name(InputPolarity polarity)
{
    switch (polarity) {
        case InputPolarity::ActiveHigh: return "active_high";
        case InputPolarity::ActiveLow: return "active_low";
        default: return "unknown";
    }
}

bool parse_polarity(const std::string& value, InputPolarity& polarity)
{
    if (value == "active_high") {
        polarity = InputPolarity::ActiveHigh;
        return true;
    }
    if (value == "active_low") {
        polarity = InputPolarity::ActiveLow;
        return true;
    }
    if (value == "unknown") {
        polarity = InputPolarity::Unknown;
        return true;
    }
    return false;
}

bool parse_json_string(const std::string& body, const char* key, std::string& value)
{
    const std::string marker = std::string{"\""} + key + "\"";
    auto pos = body.find(marker);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + marker.size());
    if (pos == std::string::npos) return false;
    pos = body.find('"', pos + 1U);
    if (pos == std::string::npos) return false;
    const auto end = body.find('"', pos + 1U);
    if (end == std::string::npos) return false;
    value.assign(body, pos + 1U, end - pos - 1U);
    return true;
}

bool read_body(httpd_req_t* request, std::size_t limit, std::string& body)
{
    if (request == nullptr || request->content_len == 0U || request->content_len > limit) return false;
    body.assign(request->content_len, '\0');
    std::size_t offset = 0;
    while (offset < body.size()) {
        const auto received = httpd_req_recv(request, body.data() + offset, body.size() - offset);
        if (received <= 0) return false;
        offset += static_cast<std::size_t>(received);
    }
    return true;
}
}

esp_err_t InputsHttp::register_handlers(
    httpd_handle_t server,
    InputRuntime* runtime,
    InputNvsStore* store,
    homeguard::AccessControl* access_control)
{
    if (server == nullptr || runtime == nullptr || store == nullptr || access_control == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    runtime_ = runtime;
    store_ = store;
    access_control_ = access_control;

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

    const httpd_uri_t routes[] = {
        {.uri="/api/v1/system/inputs", .method=HTTP_GET, .handler=&InputsHttp::inputs_get, .user_ctx=this},
        {.uri="/api/v1/system/inputs/polarity", .method=HTTP_GET, .handler=&InputsHttp::polarity_get, .user_ctx=this},
        {.uri="/api/v1/system/inputs/polarity", .method=HTTP_PUT, .handler=&InputsHttp::polarity_put, .user_ctx=this},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

esp_err_t InputsHttp::send_json(httpd_req_t* request, const std::string& body) const
{
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

esp_err_t InputsHttp::inputs_get(httpd_req_t* request)
{
    auto* self = self_from(request);
    if (self == nullptr) return ESP_FAIL;

    const int tamper = gpio_get_level(homeguard::board::kTamper);
    const int power_fail = gpio_get_level(homeguard::board::kPowerFail);

    std::ostringstream out;
    out << "{\"inputs\":["
        << "{\"index\":0,\"name\":\"Tamper\",\"gpio\":"
        << static_cast<int>(homeguard::board::kTamper)
        << ",\"rawHigh\":" << (tamper != 0 ? "true" : "false")
        << ",\"state\":\"" << (tamper != 0 ? "high" : "low") << "\"},"
        << "{\"index\":1,\"name\":\"Power Fail\",\"gpio\":"
        << static_cast<int>(homeguard::board::kPowerFail)
        << ",\"rawHigh\":" << (power_fail != 0 ? "true" : "false")
        << ",\"state\":\"" << (power_fail != 0 ? "high" : "low") << "\"}]}";
    return self->send_json(request, out.str());
}

esp_err_t InputsHttp::polarity_get(httpd_req_t* request)
{
    auto* self = self_from(request);
    if (self == nullptr || self->runtime_ == nullptr) return ESP_FAIL;
    const auto config = self->runtime_->polarity();
    std::ostringstream out;
    out << "{\"tamper\":\"" << polarity_name(config.tamper)
        << "\",\"powerFail\":\"" << polarity_name(config.power_fail) << "\"}";
    return self->send_json(request, out.str());
}

esp_err_t InputsHttp::polarity_put(httpd_req_t* request)
{
    auto* self = self_from(request);
    if (self == nullptr || self->runtime_ == nullptr || self->store_ == nullptr || self->access_control_ == nullptr) {
        return ESP_FAIL;
    }

    std::string body;
    if (!read_body(request, 512U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"invalid_body\"}");
    }

    std::string actor;
    std::string credential;
    std::string tamper_text;
    std::string power_text;
    if (!parse_json_string(body, "actor", actor) ||
        !parse_json_string(body, "credential", credential) ||
        !parse_json_string(body, "tamper", tamper_text) ||
        !parse_json_string(body, "powerFail", power_text)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"required_fields\"}");
    }

    const auto decision = self->access_control_->authorize(actor, credential, "system.inputs.configure");
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return self->send_json(request, std::string{"{\"ok\":false,\"reason\":\""} +
            homeguard::to_string(decision) + "\"}");
    }

    InputPolarityConfig config{};
    if (!parse_polarity(tamper_text, config.tamper) || !parse_polarity(power_text, config.power_fail)) {
        httpd_resp_set_status(request, "422 Unprocessable Entity");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"invalid_polarity\"}");
    }

    const auto save_error = self->store_->save(config);
    if (save_error != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"nvs_save_failed\"}");
    }

    self->runtime_->set_polarity(config);
    std::ostringstream out;
    out << "{\"ok\":true,\"tamper\":\"" << polarity_name(config.tamper)
        << "\",\"powerFail\":\"" << polarity_name(config.power_fail) << "\"}";
    return self->send_json(request, out.str());
}

}  // namespace homeguard::idf
