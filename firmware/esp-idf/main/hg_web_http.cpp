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
extern const uint8_t remotes_admin_js_start[] asm("_binary_remotes_admin_js_start");
extern const uint8_t remotes_admin_js_end[] asm("_binary_remotes_admin_js_end");
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
        {.uri="/remotes-admin.js", .method=HTTP_GET, .handler=&WebHttp::remotes_admin_js_get, .user_ctx=this},
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

/* HomeGuard-S3 canonical dashboard, cemented in firmware 2026-09-10. */
.side-foot small{display:none!important}
#cloudHeader{display:none!important}
.workspace header{height:106px!important;padding:20px 34px!important}
.workspace header h2{font-size:34px!important}
.status-grid.hg-transport-strip{display:grid!important;grid-template-columns:repeat(4,minmax(0,1fr))!important;gap:0!important;background:#fff;border:1px solid #e5e9ef;border-radius:14px;box-shadow:0 3px 12px rgba(15,32,55,.07);overflow:hidden}
.status-grid.hg-transport-strip article{min-height:112px!important;padding:15px 21px!important;border:0!important;border-right:1px solid #dce3ed!important;border-radius:0!important;box-shadow:none!important;background:#fff!important;display:flex!important;align-items:center!important;gap:17px!important}
.status-grid.hg-transport-strip article:last-child{border-right:0!important}
.hg-transport-icon{width:60px;height:60px;flex:0 0 60px;display:grid;place-items:center;font-size:41px;font-weight:800;line-height:1}
.hg-transport-icon.wifi,.hg-transport-icon.lan{color:#08aa48}.hg-transport-icon.bt,.hg-transport-icon.mqtt{color:#176bea}
.hg-transport-copy{display:grid;gap:3px;min-width:0}.hg-transport-copy>span{font-size:14px;font-weight:800;color:#111827}.hg-transport-copy>strong{font-size:18px!important;line-height:1.2;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;color:#111827}.hg-transport-copy>small{font-size:14px!important;line-height:1.2;color:#5270a3;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.hg-hidden-status{display:none!important}
.two-col.hg-dashboard-grid{display:grid!important;grid-template-columns:minmax(0,1.7fr) minmax(330px,.75fr)!important;gap:16px!important;margin-top:16px!important;align-items:start}
.hg-dashboard-grid .hg-quick-panel{grid-column:1/-1!important;min-height:0!important;padding:14px 16px 16px!important}.hg-dashboard-grid .hg-quick-panel h3{margin:0 0 10px!important}.hg-command-auth{display:none!important}
.hg-dashboard-grid .quick{grid-template-columns:repeat(4,minmax(0,1fr))!important;gap:10px!important}.hg-dashboard-grid .quick button{height:102px!important;display:grid!important;grid-template-columns:70px 1fr!important;grid-template-rows:auto auto!important;column-gap:12px!important;row-gap:2px!important;text-align:left!important;padding:12px 18px!important;justify-items:start!important;align-content:center!important}.hg-dashboard-grid .quick button b{grid-row:1/3!important;font-size:42px!important;align-self:center!important;justify-self:center!important}.hg-dashboard-grid .quick button strong{font-size:17px!important;align-self:end!important}.hg-dashboard-grid .quick button small{font-size:14px!important;align-self:start!important}
.hg-quick-secondary{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px;margin-top:10px}.hg-quick-secondary button,.hg-quick-secondary .hg-quick-state{height:88px;border:1px solid #dce3ed;border-radius:11px;background:#fff;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:4px;color:#101827}.hg-quick-secondary button{cursor:pointer}.hg-quick-secondary b{font-size:28px;line-height:1;color:#0aa43c}.hg-quick-secondary strong{font-size:16px}.hg-quick-secondary small{font-size:13px;color:#5570a0}.hg-quick-secondary button:disabled{opacity:.55;cursor:default}#hgQuickLock b{color:#111827}
.hg-dashboard-grid .panel{min-height:0!important}.hg-dashboard-grid #zones-section,.hg-dashboard-grid #io-section,.hg-dashboard-grid #events,.hg-dashboard-grid #hgDeviceInfo{padding:16px!important}.hg-dashboard-grid .panel h3{font-size:20px!important;margin:0 0 12px!important}
#zones.zones{border:0!important;border-radius:0!important;overflow:visible!important;display:grid!important;grid-template-columns:repeat(2,minmax(0,1fr))!important;grid-template-rows:repeat(4,50px)!important;grid-auto-flow:column!important;gap:4px 18px!important}
#zones .zone{height:50px!important;padding:0 12px!important;border:1px solid #e0e5ec!important;border-radius:8px!important;display:grid!important;grid-template-columns:14px 28px minmax(0,1fr) auto 14px!important;gap:9px!important;align-items:center!important;justify-content:initial!important}#zones .zone:before{display:none!important;content:none!important}.hg-zone-dot{width:11px;height:11px;border-radius:50%;background:#f0aa00}.hg-zone-dot.ok{background:#0abb54}.hg-zone-dot.alarm{background:#f34444}.hg-zone-dot.warning{background:#f0aa00}.hg-zone-id{font-weight:800;color:#111827}.hg-zone-name{min-width:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;color:#111827}.hg-zone-state{font-weight:800;font-size:13px}.hg-zone-state.ok{color:#08a63d}.hg-zone-state.alarm{color:#e52e36}.hg-zone-state.warning{color:#d88a00}.hg-zone-arrow{font-size:20px;color:#10243a}
#ioState.io{display:grid!important;grid-template-columns:repeat(2,minmax(0,1fr))!important;gap:10px!important;padding-top:0!important;align-items:stretch!important;justify-content:stretch!important}#ioState.io>div{min-height:104px!important;padding:10px!important;border:1px solid #e0e5ec;border-radius:9px;display:flex!important;flex-direction:column!important;align-items:center!important;justify-content:center!important;gap:4px!important}#ioState.io b{font-size:27px!important}#ioState.io small{color:#65728a}.hg-io-actions{display:flex;gap:6px;margin-top:3px}.hg-io-actions button{padding:5px 8px;border:1px solid #d6ddea;border-radius:6px;background:#f4f7fb;cursor:pointer}
.hg-device-table{display:grid;gap:0}.hg-device-row{min-height:31px;display:grid;grid-template-columns:125px minmax(0,1fr) 48px;gap:8px;align-items:center;border-bottom:1px solid #e1e6ee;font-size:14px}.hg-device-row:last-child{border-bottom:0}.hg-device-label{color:#5570a0}.hg-device-value{white-space:nowrap;overflow:hidden;text-overflow:ellipsis;color:#111827}.hg-health{font-weight:800;text-align:right}.hg-health.ok{color:#08a63d}.hg-health.bad{color:#e52e36}.hg-health.wait{color:#d88a00}

@media (max-width:1100px){.status-grid.hg-transport-strip{grid-template-columns:repeat(2,minmax(0,1fr))!important}.status-grid.hg-transport-strip article:nth-child(2){border-right:0!important}.status-grid.hg-transport-strip article:nth-child(-n+2){border-bottom:1px solid #dce3ed!important}.two-col.hg-dashboard-grid{grid-template-columns:1fr!important}.hg-dashboard-grid .hg-quick-panel{grid-column:1!important}.hg-dashboard-grid .quick{grid-template-columns:repeat(2,minmax(0,1fr))!important}}

/* Keep the exact release-contract rule: Bruce must never be cropped on mobile. */
.bruce img{object-fit:contain!important;object-position:center center!important}
@media (max-width:760px){
  html,body{max-width:100%;overflow-x:hidden}.shell{display:block!important;min-height:100vh}.sidebar{position:relative!important;top:auto!important;width:100%!important;height:auto!important;min-height:0!important;padding:10px!important;overflow:hidden!important}.brand{height:auto!important;min-height:34px!important;justify-content:flex-start!important;align-items:center!important;padding:0 8px!important}.brand h1{font-size:24px!important;letter-spacing:-.5px!important}.bruce{height:96px!important;max-height:96px!important;margin:4px 0 8px!important;border-radius:10px!important;overflow:hidden!important}.bruce img{object-fit:contain!important;object-position:center center!important;display:block!important;width:100%!important;height:100%!important;max-height:96px!important}.sidebar nav{display:none!important;margin:0!important;padding:0!important;flex-direction:column!important;gap:4px!important}.sidebar.mobile-nav-open nav{display:flex!important}.sidebar nav a{min-height:40px!important;margin:0!important;padding:8px 12px!important;border-radius:8px!important;font-size:14px!important;display:flex!important;align-items:center!important;gap:8px!important}#mobileNavToggle{display:block!important;width:100%!important;margin:0!important;padding:10px 12px!important;border:1px solid rgba(255,255,255,.28)!important;border-radius:8px!important;background:#173551!important;color:#fff!important;font:inherit!important;font-weight:700!important;text-align:left!important}.side-foot{display:none!important}.workspace{min-width:0!important;width:100%!important}.workspace header{height:auto!important;padding:12px 14px!important;gap:6px!important;align-items:flex-start!important;flex-wrap:wrap!important}.workspace header h2{font-size:22px!important;margin:0!important}.workspace header p{font-size:14px!important;margin:3px 0 0!important}.header-status{display:none!important}main{padding:12px!important}
  .status-grid.hg-transport-strip{grid-template-columns:1fr!important}.status-grid.hg-transport-strip article{border-right:0!important;border-bottom:1px solid #dce3ed!important;min-height:88px!important}.status-grid.hg-transport-strip article:last-child{border-bottom:0!important}.hg-dashboard-grid .quick{grid-template-columns:1fr 1fr!important}.hg-dashboard-grid .quick button{height:94px!important;grid-template-columns:48px 1fr!important;padding:9px!important}.hg-dashboard-grid .quick button b{font-size:32px!important}.hg-quick-secondary{grid-template-columns:1fr!important}#zones.zones{grid-template-columns:1fr!important;grid-template-rows:none!important;grid-auto-flow:row!important}#ioState.io{grid-template-columns:1fr 1fr!important}.hg-device-row{grid-template-columns:90px minmax(0,1fr) 42px}
  .cloud-fields{grid-template-columns:1fr!important}.lan-device{grid-template-columns:1fr!important;gap:6px!important}#networkPage .panel>div[style*="grid-template-columns:repeat(3"]{grid-template-columns:1fr!important}#networkPage .panel>div[style*="grid-template-columns:minmax(0,1fr) minmax(0,1fr) auto"]{grid-template-columns:1fr!important}#hgSessionLogout{position:static!important;display:block!important;width:calc(100% - 24px)!important;margin:12px!important;box-sizing:border-box!important}button,input,select{max-width:100%}
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
  const esc = value => String(value ?? "").replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/\"/g,"&quot;").replace(/'/g,"&#39;");
  const stateText = value => ({normal:"Норма",open:"Обрив",alarm:"Тривога",fault:"КЗ",tamper:"Тампер",bypassed:"Обхід"})[String(value||"")] || String(value||"—");
  const stateKind = value => String(value||"")==="normal" ? "ok" : (["alarm","open","tamper"].includes(String(value||"")) ? "alarm" : "warning");
  const setText = (id,value) => { const node=document.getElementById(id); if(node)node.textContent=value; };
  const setHealth = (id,ok,waiting=false) => { const node=document.getElementById(id); if(!node)return; node.textContent=waiting?"—":(ok?"OK":"ERR"); node.className=`hg-health ${waiting?"wait":(ok?"ok":"bad")}`; };

  const sidebar = document.querySelector(".sidebar");
  const bruce = sidebar?.querySelector(".bruce");
  const nav = sidebar?.querySelector("nav");
  const dashboardStatus = document.querySelector(".status-grid");
  const dashboardBody = document.querySelector(".two-col");
  const network = document.getElementById("networkPage");
  const system = document.getElementById("system");

  /* Serial is forbidden in the canonical dashboard. */
  sidebar?.querySelector(".side-foot small")?.remove();
  const cloudHeader=document.getElementById("cloudHeader"); if(cloudHeader)cloudHeader.hidden=true;

  if (dashboardStatus && !dashboardStatus.classList.contains("hg-transport-strip")) {
    dashboardStatus.classList.add("hg-transport-strip");
    dashboardStatus.innerHTML = `
      <article id="networkCard" title="Налаштування Wi-Fi"><div class="hg-transport-icon wifi">⌁</div><div class="hg-transport-copy"><span>Wi-Fi</span><strong id="wifiName">—</strong><small id="connection">IP —</small></div></article>
      <article><div class="hg-transport-icon lan">▦</div><div class="hg-transport-copy"><span>LAN</span><strong id="hgLanName">Ethernet</strong><small id="hgLanIp">IP —</small></div></article>
      <article><div class="hg-transport-icon bt">ᛒ</div><div class="hg-transport-copy"><span>BT</span><strong id="hgBtName">HomeGuard-S3</strong><small id="hgBtDetail">MAC —</small></div></article>
      <article id="cloudCard"><div class="hg-transport-icon mqtt">☁</div><div class="hg-transport-copy"><span>MQTT</span><strong id="cloudState">MQTT</strong><small id="cloudDetail">Не налаштовано</small></div></article>
      <span class="hg-hidden-status" id="zoneCount">—</span><span class="hg-hidden-status" id="securityMode">—</span>`;
  }

  if (dashboardBody) {
    dashboardBody.classList.add("hg-dashboard-grid");
    const quickPanel=dashboardBody.querySelector(":scope > .panel");
    if(quickPanel){
      quickPanel.classList.add("hg-quick-panel");
      document.getElementById("operatorId")?.parentElement?.parentElement?.classList.add("hg-command-auth");
      if(!document.getElementById("hgQuickSecondary")){
        const secondary=document.createElement("div"); secondary.id="hgQuickSecondary"; secondary.className="hg-quick-secondary";
        secondary.innerHTML=`
          <button id="hgQuickLight" type="button" data-output-id="4" data-output-active="true"><b>◯</b><strong>Освітлення</strong><small id="hgQuickLightState">OFF</small></button>
          <div class="hg-quick-state"><b>◯</b><strong>Замок</strong><small id="hgQuickLockState">OFF</small></div>
          <button id="hgQuickLock" type="button" data-output-id="5" data-output-active="true"><b>🔒</b><strong>Замок</strong><small>Відкрити на 5 секунд</small></button>`;
        quickPanel.appendChild(secondary);
      }
    }
    const events=document.getElementById("events"),io=document.getElementById("io-section");
    if(events&&io&&events.previousElementSibling!==io) events.parentElement.insertBefore(io,events);
    if(!document.getElementById("hgDeviceInfo")){
      const info=document.createElement("article"); info.id="hgDeviceInfo"; info.className="panel";
      info.innerHTML=`<h3>ⓘ &nbsp; Інформація про пристрій</h3><div class="hg-device-table">
        <div class="hg-device-row"><span class="hg-device-label">Модель</span><span class="hg-device-value" id="hgInfoModel">HomeGuard-S3</span><span></span></div>
        <div class="hg-device-row"><span class="hg-device-label">Версія ПЗ</span><span class="hg-device-value" id="hgInfoVersion">—</span><span></span></div>
        <div class="hg-device-row"><span class="hg-device-label">Час роботи</span><span class="hg-device-value" id="hgInfoUptime">—</span><span></span></div>
        <div class="hg-device-row"><span class="hg-device-label">Wi-Fi</span><span class="hg-device-value" id="hgInfoWifi">—</span><span class="hg-health wait" id="hgWifiHealth">—</span></div>
        <div class="hg-device-row"><span class="hg-device-label">LAN</span><span class="hg-device-value" id="hgInfoLan">—</span><span class="hg-health wait" id="hgLanHealth">—</span></div>
        <div class="hg-device-row"><span class="hg-device-label">Bluetooth</span><span class="hg-device-value" id="hgInfoBt">HomeGuard-S3</span><span class="hg-health wait" id="hgBtHealth">—</span></div>
        <div class="hg-device-row"><span class="hg-device-label">MQTT</span><span class="hg-device-value" id="hgInfoMqtt">—</span><span class="hg-health wait" id="hgMqttHealth">—</span></div>
      </div>`;
      dashboardBody.appendChild(info);
    }
  }

  if (sidebar && bruce && nav && !document.getElementById("mobileNavToggle")) {
    const toggle = document.createElement("button"); toggle.id="mobileNavToggle"; toggle.type="button"; toggle.textContent="☰ Меню"; toggle.setAttribute("aria-expanded","false"); bruce.insertAdjacentElement("afterend",toggle);
    const closeMenu=()=>{sidebar.classList.remove("mobile-nav-open");toggle.setAttribute("aria-expanded","false");toggle.textContent="☰ Меню";};
    toggle.addEventListener("click",()=>{const open=sidebar.classList.toggle("mobile-nav-open");toggle.setAttribute("aria-expanded",String(open));toggle.textContent=open?"✕ Закрити меню":"☰ Меню";});
    nav.querySelectorAll("a").forEach(link=>link.addEventListener("click",closeMenu));
  }

  const baseRenderZones=renderZones;
  renderZones=function(data){
    const zones=Array.isArray(data?.zones)?data.zones:[]; setText("zoneCount",zones.length||"—");
    const target=document.getElementById("zones"); if(!target)return;
    target.innerHTML=zones.length?zones.map(zone=>{const kind=stateKind(zone.state);return `<div class="zone"><i class="hg-zone-dot ${kind}"></i><b class="hg-zone-id">${Number(zone.id)||"—"}</b><span class="hg-zone-name">${esc(zone.name||`Зона ${zone.id}`)}${zone.alwaysOn?" · 24/7":""}</span><strong class="hg-zone-state ${kind}">${esc(stateText(zone.state))}</strong><span class="hg-zone-arrow">›</span></div>`;}).join(""):'<div class="zone"><span class="hg-zone-name">Дані ще не отримані</span></div>';
  };

  const baseRenderNetwork=renderNetwork;
  renderNetwork=function(status){
    baseRenderNetwork(status);
    const connected=status?.state==="connected";
    setText("connection",`IP ${status?.ip||"—"}`);
    setText("hgInfoWifi",`${status?.ssid||"—"}${status?.ip?` · ${status.ip}`:""}`);
    setHealth("hgWifiHealth",connected,false);
  };

  const baseRenderCloudStatus=renderCloudStatus;
  renderCloudStatus=function(status){
    baseRenderCloudStatus(status);
    const broker=String(status?.brokerUri||"").replace(/^mqtts?:\/\//,"").split("/")[0]||"MQTT";
    setText("cloudState",broker);
    setText("cloudDetail",status?.connected?"Підключено":(status?.configured?"Підключення…":"Не налаштовано"));
    setText("hgInfoMqtt",broker);
    setHealth("hgMqttHealth",Boolean(status?.connected),!status?.configured);
  };

  renderOutputs=function(data){
    const outputs=Array.isArray(data?.outputs)?data.outputs:[];
    const target=document.getElementById("ioState"); if(!target)return;
    const names={1:"Сирена",2:"Клапан 1",3:"Клапан 2",4:"Освітлення",5:"Замок"};
    target.innerHTML=outputs.length?outputs.map(item=>{const id=Number(item.id)||0,isValve=item.type==="valve";const controls=isValve?`<span class="hg-io-actions"><button type="button" data-output-id="${id}" data-output-active="true" ${item.active?"disabled":""}>Відкрити</button><button type="button" data-output-id="${id}" data-output-active="false" ${item.active?"":"disabled"}>Закрити</button></span>`:"";return `<div class="${item.active?"":"muted"}"><b>⇆</b><span>${esc(names[id]||`Вихід ${id}`)}</span><small>${item.active?"Увімк.":"Вимк."}</small>${controls}</div>`;}).join(""):'<div><span>Очікування реальних даних контролера…</span></div>';
    target.querySelectorAll("[data-output-id]").forEach(button=>{button.onclick=()=>sendOutputCommand(button);});
    const light=outputs.find(item=>Number(item.id)===4),lock=outputs.find(item=>Number(item.id)===5),lightButton=document.getElementById("hgQuickLight");
    if(lightButton){lightButton.dataset.outputActive=String(!Boolean(light?.active)); setText("hgQuickLightState",light?.active?"ON":"OFF");}
    setText("hgQuickLockState",lock?.active?"ON":"OFF");
  };

  async function canonicalOutput(button){
    const outputId=Number(button?.dataset?.outputId),active=button?.dataset?.outputActive==="true";
    const credentials=operatorCredentials(); if(!Number.isInteger(outputId)||outputId<=0||!validOperator(credentials.actor,credentials.credential))return;
    button.disabled=true;
    try{
      const payload={outputId,active,actor:credentials.actor,credential:credentials.credential}; if(outputId===5&&active)payload.pulseMs=5000;
      await api("/api/v1/system/output-command",{method:"POST",body:JSON.stringify(payload)});
      showToast(outputId===5?"Замок відкрито на 5 секунд":(active?"Освітлення увімкнено":"Освітлення вимкнено"));
      await refresh(); if(outputId===5)setTimeout(()=>{if(authenticatedUi())refresh();},5200);
    }catch(error){showToast(`Помилка виходу: ${error.message}`);}finally{const pin=document.getElementById("operatorPin");if(pin)pin.value="";button.disabled=false;}
  }
  document.getElementById("hgQuickLight")?.addEventListener("click",event=>canonicalOutput(event.currentTarget));
  document.getElementById("hgQuickLock")?.addEventListener("click",event=>canonicalOutput(event.currentTarget));

  function formatUptime(ms){let seconds=Math.max(0,Math.floor(Number(ms||0)/1000)),days=Math.floor(seconds/86400);seconds%=86400;const hours=Math.floor(seconds/3600),minutes=Math.floor((seconds%3600)/60);return days?`${days} д ${hours} год ${minutes} хв`:`${hours} год ${minutes} хв`;}
  async function refreshCanonicalHardware(){
    if(!authenticatedUi())return;
    try{const body=await api("/api/v1/hardware/status"),d=body?.dashboard||{},lan=d.lan||{},ble=d.ble||{};setText("hgLanName",lan.name||"Ethernet");setText("hgLanIp",`IP ${lan.ip||"—"}`);setText("hgBtName",ble.name||"HomeGuard-S3");setText("hgBtDetail",ble.address?`MAC ${ble.address}`:"MAC —");setText("hgInfoUptime",formatUptime(d.uptimeMs));setText("hgInfoLan",`${lan.name||"Ethernet"}${lan.ip?` · ${lan.ip}`:""}`);setText("hgInfoBt",`${ble.name||"HomeGuard-S3"}${ble.address?` · ${ble.address}`:""}`);setHealth("hgLanHealth",lan.state==="connected",false);setHealth("hgBtHealth",Boolean(ble.ready),false);}catch(_){setHealth("hgLanHealth",false,true);setHealth("hgBtHealth",false,true);}
  }
  async function refreshCanonicalBuild(){if(!authenticatedUi())return;try{const b=await api("/api/v1/build");setText("hgInfoModel",b.project||"HomeGuard-S3");setText("hgInfoVersion",`${b.version||"—"}${b.build?` · build ${b.build}`:""}`);}catch(_){}}

  function applyEmbeddedView(){
    const hash=window.location.hash||"#overview",isNetwork=hash==="#networkPage",isSystem=hash==="#system",hideDashboard=isNetwork||isSystem;
    [dashboardStatus,dashboardBody].forEach(section=>{if(!section)return;section.hidden=hideDashboard;if(hideDashboard)section.style.setProperty("display","none","important");else section.style.removeProperty("display");});
    if(network){network.hidden=!isNetwork;if(isNetwork)network.style.setProperty("display","block","important");else network.style.setProperty("display","none","important");}
    if(system){system.hidden=!isSystem;if(isSystem)system.style.setProperty("display","block","important");else system.style.setProperty("display","none","important");}
  }

  window.addEventListener("hashchange",applyEmbeddedView); applyEmbeddedView();
  setInterval(()=>{if(authenticatedUi())refreshCanonicalHardware();},5000);
  setInterval(()=>{if(authenticatedUi())refreshCanonicalBuild();},30000);
  setTimeout(()=>{refreshCanonicalHardware();refreshCanonicalBuild();},1200);
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

esp_err_t WebHttp::remotes_admin_js_get(httpd_req_t* request)
{
    return send_asset(request, "application/javascript; charset=utf-8", remotes_admin_js_start, remotes_admin_js_end);
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