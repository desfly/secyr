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
    // Embedded-browser fixes live in one suffix so the flashed controller does
    // not depend on user-agent quirks or stale UI assets during commissioning.
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

  function ensureWifiPasswordToggle() {
    const password = document.getElementById("wifiPassword");
    if (!password || document.getElementById("wifiPasswordToggle")) return;

    const label = password.parentElement;
    if (!label) return;
    label.style.position = "relative";
    password.style.paddingRight = "52px";

    const toggle = document.createElement("button");
    toggle.id = "wifiPasswordToggle";
    toggle.type = "button";
    toggle.textContent = "👁";
    toggle.setAttribute("aria-label", "Показати пароль Wi-Fi");
    toggle.setAttribute("aria-pressed", "false");
    toggle.title = "Показати пароль";
    toggle.style.cssText = "position:absolute;right:7px;bottom:6px;width:40px;height:34px;padding:0;border:0;background:transparent;cursor:pointer;font-size:18px;line-height:34px;text-align:center";
    toggle.addEventListener("click", () => {
      const show = password.type === "password";
      password.type = show ? "text" : "password";
      toggle.setAttribute("aria-pressed", show ? "true" : "false");
      toggle.setAttribute("aria-label", show ? "Сховати пароль Wi-Fi" : "Показати пароль Wi-Fi");
      toggle.title = show ? "Сховати пароль" : "Показати пароль";
      toggle.textContent = show ? "◉" : "👁";
      password.focus();
      const end = password.value.length;
      if (typeof password.setSelectionRange === "function") password.setSelectionRange(end, end);
    });
    label.appendChild(toggle);
  }

  function installWifiConnectHandoverFetchGuard() {
    if (window.__homeguardWifiConnectFetchGuard) return;
    const nativeFetch = window.fetch.bind(window);

    window.fetch = async (input, init = {}) => {
      const url = typeof input === "string" ? input : (input && typeof input.url === "string" ? input.url : "");
      const method = String(init.method || (input && input.method) || "GET").toUpperCase();
      const isWifiConnect = method === "POST" && (url === "/api/v1/network/connect" || url.endsWith("/api/v1/network/connect"));

      try {
        return await nativeFetch(input, init);
      } catch (error) {
        if (!isWifiConnect) throw error;

        // AP+STA may briefly retune the radio while the controller starts the
        // new STA association. That can tear down the HTTP socket even though
        // the command was accepted. Treat only this endpoint as transitional;
        // app.js will poll /network/status and still report a real timeout if
        // the controller never connects.
        const result = document.getElementById("wifiResult");
        if (result) result.textContent = "Wi-Fi перемикається, перевіряємо підключення…";
        return new Response(
          JSON.stringify({ ok: true, state: "connecting", handover: true }),
          { status: 202, headers: { "Content-Type": "application/json" } }
        );
      }
    };

    window.__homeguardWifiConnectFetchGuard = true;
  }

  window.addEventListener("hashchange", applyEmbeddedView);
  applyEmbeddedView();
  ensureWifiPasswordToggle();
  installWifiConnectHandoverFetchGuard();
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
