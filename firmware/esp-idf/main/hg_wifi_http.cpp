#include "hg_wifi_http.hpp"

#include "esp_log.h"
#include "esp_wifi.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "hg_wifi_http";

#if defined(ESP_PLATFORM)
extern const unsigned char web_index_html_start[] asm("_binary_index_html_start");
extern const unsigned char web_index_html_end[] asm("_binary_index_html_end");
extern const unsigned char web_app_css_start[] asm("_binary_app_css_start");
extern const unsigned char web_app_css_end[] asm("_binary_app_css_end");
extern const unsigned char web_app_js_start[] asm("_binary_app_js_start");
extern const unsigned char web_app_js_end[] asm("_binary_app_js_end");
#endif

std::string json_escape(const char* text)
{
    std::string out;
    if (text == nullptr) return out;
    for (const char* p = text; *p != '\0'; ++p) {
        if (*p == '"' || *p == '\\') out.push_back('\\');
        out.push_back(*p);
    }
    return out;
}

#if defined(ESP_PLATFORM)
esp_err_t send_embedded(httpd_req_t* request, const char* type, const unsigned char* start, const unsigned char* end)
{
    if (request == nullptr || start == nullptr || end == nullptr || end <= start) return ESP_ERR_INVALID_ARG;
    httpd_resp_set_type(request, type);
    const auto size = static_cast<ssize_t>((end - start) - 1);
    return httpd_resp_send(request, reinterpret_cast<const char*>(start), size);
}

esp_err_t app_css_get(httpd_req_t* request)
{
    return send_embedded(request, "text/css; charset=utf-8", web_app_css_start, web_app_css_end);
}

esp_err_t app_js_get(httpd_req_t* request)
{
    return send_embedded(request, "application/javascript; charset=utf-8", web_app_js_start, web_app_js_end);
}
#else
esp_err_t app_css_get(httpd_req_t* request)
{
    httpd_resp_set_type(request, "text/css; charset=utf-8");
    return httpd_resp_send(request, "", 0);
}

esp_err_t app_js_get(httpd_req_t* request)
{
    httpd_resp_set_type(request, "application/javascript; charset=utf-8");
    return httpd_resp_send(request, "", 0);
}
#endif

bool extract_json_string(const std::string& body, const char* key, std::string& value)
{
    const std::string token = std::string{"\""} + key + "\"";
    const auto key_pos = body.find(token); if (key_pos == std::string::npos) return false;
    const auto colon = body.find(':', key_pos + token.size()); if (colon == std::string::npos) return false;
    const auto first_quote = body.find('"', colon + 1); if (first_quote == std::string::npos) return false;
    const auto second_quote = body.find('"', first_quote + 1); if (second_quote == std::string::npos) return false;
    value = body.substr(first_quote + 1, second_quote - first_quote - 1); return true;
}
}

esp_err_t WifiProvisioningHttp::register_handlers(httpd_handle_t server, WifiCredentialStore* store, WifiProvisioningRuntime* runtime)
{
    if (server == nullptr || store == nullptr || runtime == nullptr) return ESP_ERR_INVALID_ARG;
    store_ = store; runtime_ = runtime;
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = &WifiProvisioningHttp::root_get, .user_ctx = this},
        {.uri = "/app.css", .method = HTTP_GET, .handler = &app_css_get, .user_ctx = this},
        {.uri = "/app.js", .method = HTTP_GET, .handler = &app_js_get, .user_ctx = this},
        {.uri = "/api/v1/provisioning/wifi", .method = HTTP_POST, .handler = &WifiProvisioningHttp::provision_post, .user_ctx = this},
        {.uri = "/api/v1/wifi/status", .method = HTTP_GET, .handler = &WifiProvisioningHttp::status_get, .user_ctx = this},
        {.uri = "/api/v1/wifi/scan", .method = HTTP_GET, .handler = &WifiProvisioningHttp::scan_get, .user_ctx = this},
    };
    for (const auto& route : routes) { const auto error = httpd_register_uri_handler(server, &route); if (error != ESP_OK) return error; }
    return ESP_OK;
}

WifiProvisioningHttp* WifiProvisioningHttp::self_from(httpd_req_t* request){return request == nullptr ? nullptr : static_cast<WifiProvisioningHttp*>(request->user_ctx);}
esp_err_t WifiProvisioningHttp::root_get(httpd_req_t* request)
{
#if defined(ESP_PLATFORM)
    return send_embedded(request,"text/html; charset=utf-8",web_index_html_start,web_index_html_end);
#else
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_send(request, "<!doctype html><html><body>HomeGuard-S3 Web UI host mock</body></html>", -1);
#endif
}

esp_err_t WifiProvisioningHttp::provision_post(httpd_req_t* request)
{
    auto* self=self_from(request); if(self==nullptr||request->content_len==0||request->content_len>512)return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"invalid provisioning request");
    std::array<char,513> buffer{}; const auto received=httpd_req_recv(request,buffer.data(),request->content_len); if(received<=0)return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"request body read failed");
    std::string body(buffer.data(),static_cast<std::size_t>(received)),ssid,password;
    if(!extract_json_string(body,"ssid",ssid)||ssid.empty()||ssid.size()>32||!extract_json_string(body,"password",password)||password.size()>64)return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"ssid/password invalid");
    WifiCredentials credentials{}; std::memcpy(credentials.ssid.data(),ssid.data(),ssid.size()); std::memcpy(credentials.password.data(),password.data(),password.size());
    auto error=self->store_->save(credentials); if(error!=ESP_OK){ESP_LOGE(kTag,"WiFi credentials NVS save failed: %s",esp_err_to_name(error));return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"NVS save failed");}

    httpd_resp_set_type(request,"application/json");
    const auto response_error=httpd_resp_send(request,"{\"accepted\":true,\"state\":\"connecting\"}",-1);
    if(response_error!=ESP_OK)return response_error;

    error=self->runtime_->connect_station(credentials.ssid.data(),credentials.password.data());
    if(error!=ESP_OK){ESP_LOGE(kTag,"STA connect start failed after provisioning response: %s",esp_err_to_name(error));}
    return ESP_OK;
}

esp_err_t WifiProvisioningHttp::status_get(httpd_req_t* request)
{
    auto* self=self_from(request); if(self==nullptr)return ESP_ERR_INVALID_ARG;
    const char* station_state="idle";
    if(self->runtime_->station_connected())station_state="connected";
    else if(self->runtime_->station_connecting())station_state="connecting";

    std::string body="{\"softap\":";
    body+=self->runtime_->started()?"true":"false";
    body+=",\"ssid\":\""; body+=json_escape(self->runtime_->ssid());
    body+="\",\"ip\":\""; body+=json_escape(self->runtime_->ip_address());
    body+="\",\"station\":\""; body+=station_state;
    body+="\",\"station_ssid\":\""; body+=json_escape(self->runtime_->station_ssid());
    body+="\",\"station_ip\":\""; body+=json_escape(self->runtime_->station_ip_address());
    body+="\"}";
    httpd_resp_set_type(request,"application/json"); return httpd_resp_send(request,body.c_str(),static_cast<ssize_t>(body.size()));
}

esp_err_t WifiProvisioningHttp::scan_get(httpd_req_t* request)
{
    wifi_scan_config_t config{};
    auto error=esp_wifi_scan_start(&config,true);
    if(error!=ESP_OK){ESP_LOGE(kTag,"WiFi scan failed: %s",esp_err_to_name(error));return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"wifi scan failed");}
    std::uint16_t count=0; error=esp_wifi_scan_get_ap_num(&count); if(error!=ESP_OK)return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"wifi scan count failed");
    count=std::min<std::uint16_t>(count,32); std::vector<wifi_ap_record_t> records(count); std::uint16_t fetched=count;
    if(fetched>0){error=esp_wifi_scan_get_ap_records(&fetched,records.data());if(error!=ESP_OK)return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"wifi scan records failed");}
    std::string body="{\"networks\":[";
    for(std::uint16_t i=0;i<fetched;++i){if(i)body+=',';body+="{\"ssid\":\"";body+=json_escape(reinterpret_cast<const char*>(records[i].ssid));body+="\",\"rssi\":"+std::to_string(records[i].rssi)+",\"channel\":"+std::to_string(records[i].primary)+"}";}
    body+="]}"; httpd_resp_set_type(request,"application/json"); return httpd_resp_send(request,body.c_str(),static_cast<ssize_t>(body.size()));
}

}  // namespace homeguard::idf
