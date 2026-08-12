#include "hg_web_http.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/types.h>

// ESP-IDF embed symbols use the copied asset basename.
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t app_css_start[] asm("_binary_app_css_start");
extern const uint8_t app_css_end[] asm("_binary_app_css_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");
extern const uint8_t access_session_js_start[] asm("_binary_access_session_js_start");
extern const uint8_t access_session_js_end[] asm("_binary_access_session_js_end");
extern const uint8_t bruce_jpg_start[] asm("_binary_bruce_jpg_start");
extern const uint8_t bruce_jpg_end[] asm("_binary_bruce_jpg_end");

namespace homeguard::idf {
namespace {

std::size_t text_asset_size(const uint8_t* start, const uint8_t* end)
{
    if (start == nullptr || end == nullptr || end < start) return 0;
    std::size_t size = static_cast<std::size_t>(end - start);
    // EMBED_TXTFILES may append a terminating NUL. Never place injected CSS/JS
    // after that byte: browsers can treat the remainder as unreachable text.
    while (size > 0 && start[size - 1U] == 0U) --size;
    return size;
}

esp_err_t send_text_with_suffix(httpd_req_t* request,
                                const char* content_type,
                                const uint8_t* start,
                                const uint8_t* end,
                                const char* suffix,
                                std::size_t suffix_size)
{
    if (request == nullptr || start == nullptr || end == nullptr || end < start) return ESP_ERR_INVALID_ARG;

    const auto base_size = text_asset_size(start, end);
    std::string body(reinterpret_cast<const char*>(start), base_size);
    if (suffix != nullptr && suffix_size > 0) body.append(suffix, suffix_size);

    httpd_resp_set_hdr(request, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(request, "Pragma", "no-cache");
    httpd_resp_set_hdr(request, "Expires", "0");
    httpd_resp_set_type(request, content_type);
    return httpd_resp_send(request, body.data(), static_cast<ssize_t>(body.size()));
}

}  // namespace

esp_err_t WebHttp::register_handlers(httpd_handle_t server)
{
    if (server == nullptr) return ESP_ERR_INVALID_ARG;

    const httpd_uri_t routes[] = {
        {.uri="/", .method=HTTP_GET, .handler=&WebHttp::index_get, .user_ctx=this},
        {.uri="/index.html", .method=HTTP_GET, .handler=&WebHttp::index_get, .user_ctx=this},
        {.uri="/app.css", .method=HTTP_GET, .handler=&WebHttp::css_get, .user_ctx=this},
        {.uri="/app.js", .method=HTTP_GET, .handler=&WebHttp::js_get, .user_ctx=this},
        {.uri="/access-session.js", .method=HTTP_GET, .handler=&WebHttp::access_session_js_get, .user_ctx=this},
        {.uri="/bruce.jpg", .method=HTTP_GET, .handler=&WebHttp::bruce_get, .user_ctx=this},
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

    // HomeGuard is repeatedly reflashed during commissioning. Stable asset
    // URLs must never let an old browser cache mask a freshly flashed UI.
    httpd_resp_set_hdr(request, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(request, "Pragma", "no-cache");
    httpd_resp_set_hdr(request, "Expires", "0");
    httpd_resp_set_type(request, content_type);
    return httpd_resp_send(
        request,
        reinterpret_cast<const char*>(start),
        static_cast<ssize_t>(end - start));
}

esp_err_t WebHttp::index_get(httpd_req_t* request)
{
    return send_asset(request, "text/html; charset=utf-8", index_html_start, index_html_end);
}

esp_err_t WebHttp::css_get(httpd_req_t* request)
{
    static constexpr char kHiddenVisibilityFix[] = "\n[hidden]{display:none!important}\n";
    return send_text_with_suffix(
        request,
        "text/css; charset=utf-8",
        app_css_start,
        app_css_end,
        kHiddenVisibilityFix,
        sizeof(kHiddenVisibilityFix) - 1U);
}

esp_err_t WebHttp::js_get(httpd_req_t* request)
{
    // Belt-and-suspenders routing for the real embedded browser. The normal
    // app router still owns navigation; this handler only enforces actual
    // visibility with inline !important styles after every hash change.
    static constexpr char kEmbeddedViewFix[] = R"JS(

;(() => {
  const dashboardStatus = document.querySelector(".status-grid");
  const dashboardBody = document.querySelector(".two-col");
  const network = document.getElementById("networkPage");
  const system = document.getElementById("system");

  function applyEmbeddedView() {
    const hash = window.location.hash || "#overview";
    const isNetwork = hash === "#networkPage";
    const isSystem = hash === "#system";
    const hideDashboard = isNetwork || isSystem;

    [dashboardStatus, dashboardBody].forEach((section) => {
      if (!section) return;
      section.hidden = hideDashboard;
      if (hideDashboard) section.style.setProperty("display", "none", "important");
      else section.style.removeProperty("display");
    });

    if (network) {
      network.hidden = !isNetwork;
      if (isNetwork) network.style.setProperty("display", "block", "important");
      else network.style.setProperty("display", "none", "important");
    }

    if (system) {
      system.hidden = !isSystem;
      if (isSystem) system.style.setProperty("display", "block", "important");
      else system.style.setProperty("display", "none", "important");
    }
  }

  window.addEventListener("hashchange", applyEmbeddedView);
  applyEmbeddedView();
})();
)JS";

    return send_text_with_suffix(
        request,
        "application/javascript; charset=utf-8",
        app_js_start,
        app_js_end,
        kEmbeddedViewFix,
        sizeof(kEmbeddedViewFix) - 1U);
}

esp_err_t WebHttp::access_session_js_get(httpd_req_t* request)
{
    return send_asset(request, "application/javascript; charset=utf-8", access_session_js_start, access_session_js_end);
}

esp_err_t WebHttp::bruce_get(httpd_req_t* request)
{
    return send_asset(request, "image/jpeg", bruce_jpg_start, bruce_jpg_end);
}

}  // namespace homeguard::idf