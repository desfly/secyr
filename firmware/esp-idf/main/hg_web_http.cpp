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

  const zoneStateMeta = {
    normal: {color:"#22c55e"},
    open: {color:"#ef4444"},
    short: {color:"#eab308"}
  };
  const zoneEscape = (value) => String(value ?? "").replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/\"/g,"&quot;").replace(/'/g,"&#39;");
  let zoneRefreshBusy = false;

  async function saveZoneName(id, currentName) {
    if (window.HomeGuardAuth?.role?.() !== "admin") return;
    const nextName = window.prompt(`Назва зони Z${id}`, currentName || `Зона ${id}`);
    if (nextName === null) return;
    const name = nextName.trim();
    if (!name || name.length > 23) {
      if (typeof showToast === "function") showToast("Назва має містити 1–23 символи");
      return;
    }
    const response = await fetch("/api/v1/zones/name", {
      method:"POST",
      cache:"no-store",
      headers:{"Content-Type":"application/json"},
      body:JSON.stringify({id:String(id), name, actor:window.HomeGuardAuth?.actor?.() || ""})
    });
    if (!response.ok) {
      if (typeof showToast === "function") showToast("Не вдалося зберегти назву зони");
      return;
    }
    await refreshZoneLive();
  }

  async function refreshZoneLive() {
    if (zoneRefreshBusy || window.HomeGuardAuth?.authenticated?.() !== true) return;
    const target = document.getElementById("zones");
    if (!target) return;
    zoneRefreshBusy = true;
    try {
      const response = await fetch("/api/v1/zones/live", {cache:"no-store"});
      if (!response.ok) return;
      const body = await response.json();
      const zones = Array.isArray(body?.zones) ? body.zones : [];
      const admin = window.HomeGuardAuth?.role?.() === "admin";
      const count = document.getElementById("zoneCount");
      if (count) count.textContent = String(zones.length || 8);

      target.style.display = "grid";
      target.style.gridTemplateColumns = "repeat(2,minmax(0,1fr))";
      target.style.gap = "0 12px";
      target.innerHTML = zones.map((zone) => {
        const meta = zoneStateMeta[zone.state] || zoneStateMeta.open;
        const edit = admin ? ` title="Натисніть, щоб змінити назву" style="cursor:pointer"` : "";
        return `<div class="zone hg-zone-live" data-zone-id="${Number(zone.id)}" style="display:grid;grid-template-columns:14px 34px minmax(0,1fr);gap:8px;align-items:center;min-height:42px;padding:0 4px">
          <i aria-label="${zoneEscape(zone.state || "open")}" style="display:inline-block;width:11px;height:11px;border-radius:50%;background:${meta.color}"></i>
          <b>Z${Number(zone.id)}</b>
          <span class="hg-zone-name"${edit}>${zoneEscape(zone.name || `Зона ${zone.id}`)}</span>
        </div>`;
      }).join("");
      if (admin) {
        target.querySelectorAll(".hg-zone-name").forEach((node) => {
          node.addEventListener("click", () => {
            const row = node.closest("[data-zone-id]");
            saveZoneName(Number(row?.dataset.zoneId || 0), node.textContent || "");
          });
        });
      }
    } catch (_) {
    } finally {
      zoneRefreshBusy = false;
    }
  }

  // The base app refresh loop used to repaint #zones from /api/v1/system/zones
  // (the legacy 2-zone model). Redirect that renderer to the live 8-zone source
  // so the old 5-second refresh can no longer overwrite the ADS1115 view.
  if (typeof renderZones === "function") {
    renderZones = () => { void refreshZoneLive(); };
  }

  window.addEventListener("hashchange", applyEmbeddedView);
  applyEmbeddedView();
  setInterval(refreshZoneLive, 1000);
  setTimeout(refreshZoneLive, 500);
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
