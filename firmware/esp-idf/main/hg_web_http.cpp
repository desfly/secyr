#include "hg_web_http.hpp"

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t app_css_start[] asm("_binary_app_css_start");
extern const uint8_t app_css_end[] asm("_binary_app_css_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");
extern const uint8_t access_session_js_start[] asm("_binary_access_session_js_start");
extern const uint8_t access_session_js_end[] asm("_binary_access_session_js_end");
extern const uint8_t factory_reset_js_start[] asm("_binary_factory_reset_js_start");
extern const uint8_t factory_reset_js_end[] asm("_binary_factory_reset_js_end");
extern const uint8_t bruce_jpg_start[] asm("_binary_bruce_jpg_start");
extern const uint8_t bruce_jpg_end[] asm("_binary_bruce_jpg_end");

namespace homeguard::idf {
namespace {

std::size_t text_asset_size(const uint8_t* start, const uint8_t* end)
{
    if (start == nullptr || end == nullptr || end < start) return 0;
    std::size_t size = static_cast<std::size_t>(end - start);
    while (size > 0 && start[size - 1U] == 0U) --size;
    return size;
}

void set_no_cache_headers(httpd_req_t* request, const char* content_type)
{
    httpd_resp_set_hdr(request, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(request, "Pragma", "no-cache");
    httpd_resp_set_hdr(request, "Expires", "0");
    httpd_resp_set_type(request, content_type);
}

esp_err_t send_text_with_suffix(httpd_req_t* request,
                                const char* content_type,
                                const uint8_t* start,
                                const uint8_t* end,
                                const char* suffix,
                                std::size_t suffix_size)
{
    if (request == nullptr || start == nullptr || end == nullptr || end < start) return ESP_ERR_INVALID_ARG;
    set_no_cache_headers(request, content_type);

    const auto base_size = text_asset_size(start, end);
    auto error = httpd_resp_send_chunk(
        request,
        reinterpret_cast<const char*>(start),
        static_cast<ssize_t>(base_size));
    if (error != ESP_OK) return error;

    if (suffix != nullptr && suffix_size > 0) {
        error = httpd_resp_send_chunk(request, suffix, static_cast<ssize_t>(suffix_size));
        if (error != ESP_OK) return error;
    }
    return httpd_resp_send_chunk(request, nullptr, 0);
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
        {.uri="/factory-reset.js", .method=HTTP_GET, .handler=&WebHttp::factory_reset_js_get, .user_ctx=this},
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
    set_no_cache_headers(request, content_type);
    return httpd_resp_send(request, reinterpret_cast<const char*>(start), static_cast<ssize_t>(end - start));
}

esp_err_t WebHttp::index_get(httpd_req_t* request)
{
    return send_asset(request, "text/html; charset=utf-8", index_html_start, index_html_end);
}

esp_err_t WebHttp::css_get(httpd_req_t* request)
{
    static constexpr char kFirmwareCssFix[] = R"CSS(

[hidden]{display:none!important}
#mobileNavToggle{display:none}
@media (max-width:760px){
  html,body{max-width:100%;overflow-x:hidden}
  .shell{display:block!important;min-height:100vh}
  .sidebar{position:relative!important;top:auto!important;width:100%!important;height:auto!important;min-height:0!important;padding:10px!important;overflow:hidden!important}
  .brand{height:auto!important;min-height:34px!important;justify-content:flex-start!important;align-items:center!important;padding:0 8px!important}
  .brand h1{font-size:24px!important;letter-spacing:-.5px!important}
  .bruce{height:96px!important;max-height:96px!important;margin:4px 0 8px!important;border-radius:10px!important;overflow:hidden!important}
  .bruce img{object-fit:contain!important;object-position:center center!important}
  .bruce img{display:block!important;width:100%!important;height:100%!important;max-height:96px!important}
  .sidebar nav{display:none!important;margin:0!important;padding:0!important;flex-direction:column!important;gap:4px!important}
  .sidebar.mobile-nav-open nav{display:flex!important}
  .sidebar nav a{min-height:40px!important;margin:0!important;padding:8px 12px!important;border-radius:8px!important;font-size:14px!important;display:flex!important;align-items:center!important;gap:8px!important}
  #mobileNavToggle{display:block!important;width:100%!important;margin:0!important;padding:10px 12px!important;border:1px solid rgba(255,255,255,.28)!important;border-radius:8px!important;background:#173551!important;color:#fff!important;font:inherit!important;font-weight:700!important;text-align:left!important}
  .side-foot{display:none!important}
  .workspace{min-width:0!important;width:100%!important}
  .workspace header{padding:12px 14px!important;gap:6px!important;align-items:flex-start!important;flex-wrap:wrap!important}
  .workspace header h2{font-size:22px!important;margin:0!important}
  .workspace header p{font-size:14px!important;margin:3px 0 0!important}
  .header-status{display:none!important}
  main{padding:12px!important}
  .status-grid,.two-col{grid-template-columns:1fr!important;gap:10px!important}
  .status-grid article,.panel{min-width:0!important}
  .status-grid article{min-height:104px!important;padding:14px!important}
  .quick{grid-template-columns:repeat(2,minmax(0,1fr))!important}
  .cloud-fields{grid-template-columns:1fr!important}
  .lan-device{grid-template-columns:1fr!important;gap:6px!important}
  #networkPage .panel>div[style*="grid-template-columns:repeat(3"]{grid-template-columns:1fr!important}
  #networkPage .panel>div[style*="grid-template-columns:minmax(0,1fr) minmax(0,1fr) auto"]{grid-template-columns:1fr!important}
  #hgSessionLogout{position:static!important;display:block!important;width:calc(100% - 24px)!important;margin:12px!important;box-sizing:border-box!important}
  button,input,select{max-width:100%}
}
)CSS";
    return send_text_with_suffix(
        request,
        "text/css; charset=utf-8",
        app_css_start,
        app_css_end,
        kFirmwareCssFix,
        sizeof(kFirmwareCssFix) - 1U);
}

esp_err_t WebHttp::js_get(httpd_req_t* request)
{
    static constexpr char kEmbeddedViewFix[] = R"JS(

;(() => {
  const dashboardStatus = document.querySelector(".status-grid");
  const dashboardBody = document.querySelector(".two-col");
  const network = document.getElementById("networkPage");
  const system = document.getElementById("system");
  const sidebar = document.querySelector(".sidebar");
  const bruce = sidebar?.querySelector(".bruce");
  const nav = sidebar?.querySelector("nav");

  if (sidebar && bruce && nav && !document.getElementById("mobileNavToggle")) {
    const toggle = document.createElement("button");
    toggle.id = "mobileNavToggle";
    toggle.type = "button";
    toggle.textContent = "☰ Меню";
    toggle.setAttribute("aria-expanded", "false");
    bruce.insertAdjacentElement("afterend", toggle);
    const closeMenu = () => {
      sidebar.classList.remove("mobile-nav-open");
      toggle.setAttribute("aria-expanded", "false");
      toggle.textContent = "☰ Меню";
    };
    toggle.addEventListener("click", () => {
      const open = sidebar.classList.toggle("mobile-nav-open");
      toggle.setAttribute("aria-expanded", String(open));
      toggle.textContent = open ? "✕ Закрити меню" : "☰ Меню";
    });
    nav.querySelectorAll("a").forEach(link => link.addEventListener("click", closeMenu));
  }

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

;(() => {
  let timer = 0;

  const label = (ok, pending = false) => pending ? "ПІДКЛЮЧЕННЯ" : (ok ? "ОНЛАЙН" : "ОФЛАЙН");
  const badge = (ok, pending = false) => {
    const text = label(ok, pending);
    const bg = pending ? "#fff4cc" : (ok ? "#dcf8e8" : "#ffe3e3");
    const fg = pending ? "#8a6500" : (ok ? "#157347" : "#b42318");
    return `<strong style="display:inline-block;min-width:104px;padding:5px 9px;border-radius:999px;text-align:center;background:${bg};color:${fg}">${text}</strong>`;
  };

  function removeModule() {
    document.getElementById("adminConnectivityCard")?.remove();
    const wifi = document.getElementById("networkCard");
    const cloud = document.getElementById("cloudCard");
    if (wifi) wifi.hidden = false;
    if (cloud) cloud.hidden = false;
    if (timer) { clearInterval(timer); timer = 0; }
  }

  function ensureModule() {
    if (window.HomeGuardAuth?.role?.() !== "admin") { removeModule(); return null; }
    const grid = document.querySelector(".status-grid");
    if (!grid) return null;
    let card = document.getElementById("adminConnectivityCard");
    if (!card) {
      document.getElementById("networkCard")?.setAttribute("hidden", "");
      document.getElementById("cloudCard")?.setAttribute("hidden", "");
      card = document.createElement("article");
      card.id = "adminConnectivityCard";
      card.style.cssText = "grid-column:span 2;display:block;min-height:0;padding:16px 18px";
      card.innerHTML = `
        <div style="display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:10px">
          <div><span style="font-size:13px;color:#66758b">Admin</span><strong style="display:block;font-size:18px">Зв'язок</strong></div>
          <small id="adminConnectivityUpdated">—</small>
        </div>
        <div style="display:grid;grid-template-columns:minmax(120px,1fr) 130px minmax(120px,1fr);gap:0;border:1px solid #d7deea;border-radius:10px;overflow:hidden">
          <div style="padding:8px 10px;background:#f5f7fa;font-weight:700">Канал</div><div style="padding:8px 10px;background:#f5f7fa;font-weight:700">Стан</div><div style="padding:8px 10px;background:#f5f7fa;font-weight:700">IP / адреса</div>
          <div style="padding:10px;border-top:1px solid #e2e7ef;font-weight:700">Wi‑Fi</div><div id="commWifiState" style="padding:10px;border-top:1px solid #e2e7ef">—</div><div id="commWifiIp" style="padding:10px;border-top:1px solid #e2e7ef">—</div>
          <div style="padding:10px;border-top:1px solid #e2e7ef;font-weight:700">Ethernet W5500</div><div id="commEthState" style="padding:10px;border-top:1px solid #e2e7ef">—</div><div id="commEthIp" style="padding:10px;border-top:1px solid #e2e7ef">—</div>
          <div style="padding:10px;border-top:1px solid #e2e7ef;font-weight:700">Bluetooth BLE</div><div id="commBleState" style="padding:10px;border-top:1px solid #e2e7ef">—</div><div id="commBleIp" style="padding:10px;border-top:1px solid #e2e7ef">—</div>
          <div style="padding:10px;border-top:1px solid #e2e7ef;font-weight:700">Cloud MQTT</div><div id="commCloudState" style="padding:10px;border-top:1px solid #e2e7ef">—</div><div id="commCloudIp" style="padding:10px;border-top:1px solid #e2e7ef">—</div>
        </div>`;
      grid.appendChild(card);
    }
    return card;
  }

  async function getJson(path) {
    const response = await fetch(path, {cache:"no-store"});
    const text = await response.text();
    let body = {};
    try { body = text ? JSON.parse(text) : {}; } catch (_) {}
    if (!response.ok || body.ok === false) throw new Error(body.reason || String(response.status));
    return body;
  }

  async function refreshConnectivity() {
    if (window.HomeGuardAuth?.role?.() !== "admin") { removeModule(); return; }
    if (!ensureModule()) return;
    try {
      const [wifi, local, cloud] = await Promise.all([
        getJson("/api/v1/network/status"),
        getJson("/api/v1/connectivity/status"),
        getJson("/api/v1/cloud/status")
      ]);
      const wifiConnected = wifi?.state === "connected";
      const wifiPending = wifi?.state === "connecting";
      document.getElementById("commWifiState").innerHTML = badge(wifiConnected, wifiPending);
      document.getElementById("commWifiIp").textContent = wifi?.ip || "—";

      const ethConnected = local?.ethernet?.linkUp === true && local?.ethernet?.hasIp === true;
      document.getElementById("commEthState").innerHTML = badge(ethConnected, local?.ethernet?.initialized === true && !ethConnected);
      document.getElementById("commEthIp").textContent = local?.ethernet?.ip || "—";

      const bleConnected = local?.ble?.connected === true;
      document.getElementById("commBleState").innerHTML = badge(bleConnected, false);
      document.getElementById("commBleIp").textContent = "—";

      const cloudConnected = cloud?.connected === true;
      const cloudPending = cloud?.configured === true && !cloudConnected;
      document.getElementById("commCloudState").innerHTML = badge(cloudConnected, cloudPending);
      document.getElementById("commCloudIp").textContent = cloud?.deviceId || "—";

      document.getElementById("adminConnectivityUpdated").textContent = `Оновлено ${new Date().toLocaleTimeString("uk-UA")}`;
    } catch (error) {
      const updated = document.getElementById("adminConnectivityUpdated");
      if (updated) updated.textContent = `Помилка: ${error.message}`;
    }
  }

  const boot = setInterval(() => {
    if (!window.HomeGuardAuth?.authenticated?.()) return;
    clearInterval(boot);
    if (window.HomeGuardAuth.role() !== "admin") { removeModule(); return; }
    ensureModule();
    refreshConnectivity();
    timer = setInterval(refreshConnectivity, 5000);
  }, 250);
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
    return send_text_with_suffix(
        request,
        "application/javascript; charset=utf-8",
        access_session_js_start,
        access_session_js_end,
        reinterpret_cast<const char*>(factory_reset_js_start),
        text_asset_size(factory_reset_js_start, factory_reset_js_end));
}

esp_err_t WebHttp::factory_reset_js_get(httpd_req_t* request)
{
    return send_asset(request, "application/javascript; charset=utf-8", factory_reset_js_start, factory_reset_js_end);
}

esp_err_t WebHttp::bruce_get(httpd_req_t* request)
{
    return send_asset(request, "image/jpeg", bruce_jpg_start, bruce_jpg_end);
}

}  // namespace homeguard::idf