#include "hg_web_http.hpp"

#include <cstddef>

namespace homeguard::idf {
namespace {

extern const unsigned char web_index_html_start[] asm("_binary_web_index_html_start");
extern const unsigned char web_index_html_end[] asm("_binary_web_index_html_end");
extern const unsigned char web_app_css_start[] asm("_binary_web_app_css_start");
extern const unsigned char web_app_css_end[] asm("_binary_web_app_css_end");
extern const unsigned char web_app_js_start[] asm("_binary_web_app_js_start");
extern const unsigned char web_app_js_end[] asm("_binary_web_app_js_end");

}  // namespace

esp_err_t WebHttp::register_handlers(httpd_handle_t server)
{
    if (server == nullptr) return ESP_ERR_INVALID_ARG;

    const httpd_uri_t routes[] = {
        {.uri="/", .method=HTTP_GET, .handler=&WebHttp::index_get, .user_ctx=this},
        {.uri="/index.html", .method=HTTP_GET, .handler=&WebHttp::index_get, .user_ctx=this},
        {.uri="/app.css", .method=HTTP_GET, .handler=&WebHttp::css_get, .user_ctx=this},
        {.uri="/app.js", .method=HTTP_GET, .handler=&WebHttp::js_get, .user_ctx=this},
    };

    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

esp_err_t WebHttp::send_asset(httpd_req_t* request,
                              const char* content_type,
                              const unsigned char* start,
                              const unsigned char* end)
{
    if (request == nullptr || start == nullptr || end == nullptr || end < start) return ESP_ERR_INVALID_ARG;
    httpd_resp_set_type(request, content_type);
    httpd_resp_set_hdr(request, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(request, "Pragma", "no-cache");
    return httpd_resp_send(
        request,
        reinterpret_cast<const char*>(start),
        static_cast<ssize_t>(end - start));
}

esp_err_t WebHttp::index_get(httpd_req_t* request)
{
    return send_asset(request, "text/html; charset=utf-8", web_index_html_start, web_index_html_end);
}

esp_err_t WebHttp::css_get(httpd_req_t* request)
{
    return send_asset(request, "text/css; charset=utf-8", web_app_css_start, web_app_css_end);
}

esp_err_t WebHttp::js_get(httpd_req_t* request)
{
    return send_asset(request, "application/javascript; charset=utf-8", web_app_js_start, web_app_js_end);
}

}  // namespace homeguard::idf
