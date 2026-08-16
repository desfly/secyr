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
    static constexpr char kFirmwareCssFix[] = R"CSS(

[hidden]{display:none!important}
.mobile-menu-toggle{display:none}
.hg-factory-panel{max-width:920px;margin-top:18px}
.hg-factory-fields{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:12px;margin-top:12px}
.hg-factory-fields label{display:block}
.hg-factory-fields input{display:block;width:100%;margin-top:6px;padding:11px;border:1px solid #d7deea;border-radius:8px;box-sizing:border-box}
.hg-secret-wrap{position:relative}
.hg-secret-wrap input{padding-right:52px!important}
.hg-secret-toggle{position:absolute;right:7px;bottom:6px;width:40px;height:34px;padding:0;border:0;background:transparent;cursor:pointer;font-size:18px;line-height:34px;text-align:center}
.hg-danger{border-color:#b42318!important;color:#b42318!important}
@media (max-width:760px){
  html,body{max-width:100%;overflow-x:hidden}
  .shell{display:block!important;min-height:100vh}
  .sidebar{position:relative!important;top:auto!important;width:100%!important;height:auto!important;min-height:0!important;padding:10px 10px 12px!important;overflow:visible!important}
  .brand{height:auto!important;min-height:34px!important;justify-content:center!important;align-items:center!important;padding:0 8px!important}
  .brand h1{font-size:24px!important;letter-spacing:-.5px!important}
  .bruce{height:150px!important;margin:4px 8px 8px!important;border-radius:10px!important;overflow:hidden!important;background:none!important}
  .bruce img{display:block!important;width:100%!important;height:100%!important;object-fit:contain!important;object-position:center center!important}
  .mobile-menu-toggle{display:flex!important;width:100%!important;min-height:44px!important;margin:0 0 8px!important;padding:10px 14px!important;align-items:center!important;justify-content:space-between!important;border:1px solid rgba(255,255,255,.22)!important;border-radius:10px!important;background:rgba(255,255,255,.08)!important;color:#fff!important;font:inherit!important;font-weight:700!important}
  .sidebar nav{display:none!important;position:static!important;width:100%!important;max-height:none!important;overflow:visible!important;margin:0!important;padding:0!important;z-index:auto!important}
  .sidebar.mobile-menu-open nav{display:grid!important;grid-template-columns:repeat(2,minmax(0,1fr))!important;gap:6px!important}
  .sidebar nav a{min-height:44px!important;margin:0!important;padding:10px 12px!important;border-radius:10px!important;font-size:15px!important;align-items:center!important;gap:8px!important;min-width:0!important}
  .side-foot{display:none!important}
  .workspace{min-width:0!important;width:100%!important}
  .workspace header{padding:14px 14px 10px!important;gap:8px!important;align-items:flex-start!important;flex-wrap:wrap!important}
  .workspace header h2{font-size:24px!important;margin:0!important}
  .workspace header p{margin:3px 0 0!important}
  .header-status{width:100%!important;justify-content:space-between!important;gap:8px!important}
  main{padding:12px!important}
  .status-grid,.two-col{grid-template-columns:1fr!important;gap:10px!important}
  .status-grid article,.panel{min-width:0!important}
  .quick{grid-template-columns:repeat(2,minmax(0,1fr))!important}
  .cloud-fields,.hg-factory-fields{grid-template-columns:1fr!important}
  .lan-device{grid-template-columns:1fr!important;gap:6px!important}
  #networkPage .panel>div[style*="grid-template-columns:repeat(3"]{grid-template-columns:1fr!important}
  #networkPage .panel>div[style*="grid-template-columns:minmax(0,1fr) minmax(0,1fr) auto"]{grid-template-columns:1fr!important}
  button,input,select{max-width:100%}
}
@media (max-width:430px){
  .sidebar.mobile-menu-open nav{grid-template-columns:1fr!important}
  .bruce{height:132px!important}
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
  let lastSidebarLink = document.querySelector(".sidebar nav a.active") || null;
  let bootstrapProbeInFlight = false;
  let bootstrapAvailable = null;

  function applyEmbeddedView() {
    const hash = window.location.hash || "#overview";
    const isNetwork = hash === "#networkPage";
    const isSystem = hash === "#system";
    const hideDashboard = isNetwork || isSystem;

    [dashboardStatus, dashboardBody].forEach(section => {
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

  function enforceSingleActiveNav() {
    const links = [...document.querySelectorAll(".sidebar nav a")];
    if (!links.length) return;
    const hash = window.location.hash || "#overview";
    const matches = links.filter(link => link.getAttribute("href") === hash);

    let preferred = null;
    if (lastSidebarLink && lastSidebarLink.isConnected && links.includes(lastSidebarLink)) {
      if (lastSidebarLink.getAttribute("href") === hash) preferred = lastSidebarLink;
    }
    if (!preferred && matches.length) preferred = matches[0];
    if (!preferred) preferred = links.find(link => link.classList.contains("active")) || links[0];

    links.forEach(link => link.classList.toggle("active", link === preferred));
    lastSidebarLink = preferred;
  }

  function ensureFirmwareMobileNavigation() {
    const sidebar = document.querySelector(".sidebar");
    const bruce = document.querySelector(".sidebar .bruce");
    const nav = document.querySelector(".sidebar nav");
    if (!sidebar || !bruce || !nav) return;

    let toggle = document.getElementById("mobileMenuToggle");
    if (!toggle) {
      toggle = document.createElement("button");
      toggle.id = "mobileMenuToggle";
      toggle.type = "button";
      toggle.className = "mobile-menu-toggle";
      toggle.setAttribute("aria-expanded", "false");
      toggle.setAttribute("aria-controls", "homeguardSidebarNav");
      toggle.innerHTML = "<span>☰ Меню</span><span aria-hidden=\"true\">⌄</span>";
      nav.id = nav.id || "homeguardSidebarNav";
      bruce.insertAdjacentElement("afterend", toggle);
      toggle.addEventListener("click", () => {
        const open = !sidebar.classList.contains("mobile-menu-open");
        sidebar.classList.toggle("mobile-menu-open", open);
        toggle.setAttribute("aria-expanded", open ? "true" : "false");
        if (toggle.lastElementChild) toggle.lastElementChild.textContent = open ? "⌃" : "⌄";
      });
    }

    if (nav.dataset.hgActiveBound !== "1") {
      nav.dataset.hgActiveBound = "1";
      nav.addEventListener("click", event => {
        const link = event.target.closest?.("a");
        if (!link) return;
        lastSidebarLink = link;
        [...nav.querySelectorAll("a")].forEach(item => item.classList.toggle("active", item === link));
        queueMicrotask(enforceSingleActiveNav);
        setTimeout(enforceSingleActiveNav, 0);
        if (window.matchMedia("(max-width:760px)").matches) {
          sidebar.classList.remove("mobile-menu-open");
          toggle.setAttribute("aria-expanded", "false");
          if (toggle.lastElementChild) toggle.lastElementChild.textContent = "⌄";
        }
      });
    }

    window.addEventListener("resize", () => {
      if (!window.matchMedia("(max-width:760px)").matches) {
        sidebar.classList.remove("mobile-menu-open");
        toggle.setAttribute("aria-expanded", "false");
      }
    }, { passive: true });
  }

  function addSecretToggle(input) {
    if (!input || input.dataset.hgSecretToggle === "1") return;
    input.dataset.hgSecretToggle = "1";
    const parent = input.parentElement;
    if (!parent) return;
    parent.classList.add("hg-secret-wrap");

    const toggle = document.createElement("button");
    toggle.type = "button";
    toggle.className = "hg-secret-toggle";
    toggle.textContent = "👁";
    toggle.setAttribute("aria-label", "Показати");
    toggle.title = "Показати";
    toggle.addEventListener("click", event => {
      event.preventDefault();
      event.stopPropagation();
      const show = input.type === "password";
      input.type = show ? "text" : "password";
      toggle.textContent = show ? "◉" : "👁";
      toggle.title = show ? "Сховати" : "Показати";
      toggle.setAttribute("aria-label", toggle.title);
      input.focus();
      const end = input.value.length;
      if (typeof input.setSelectionRange === "function") input.setSelectionRange(end, end);
    });
    parent.appendChild(toggle);
  }

  function ensureSecretToggles() {
    ["operatorPin", "wifiPassword", "cloudPassword", "cloudCredential", "networkCredential", "accessCredential", "factoryCredential"]
      .forEach(id => addSecretToggle(document.getElementById(id)));
  }

  function applyBootstrapVisibility(button) {
    if (!button) return;
    const visible = bootstrapAvailable === true;
    button.hidden = !visible;
    button.setAttribute("aria-hidden", visible ? "false" : "true");
    if (visible) button.removeAttribute("tabindex");
    else button.tabIndex = -1;

    const row = button.parentElement;
    const hint = row?.nextElementSibling;
    if (hint && hint.tagName === "SMALL" && hint.textContent?.includes("першого Admin")) {
      hint.hidden = !visible;
      hint.setAttribute("aria-hidden", visible ? "false" : "true");
    }
  }

  async function syncBootstrapAvailability() {
    const button = document.getElementById("accessBootstrap");
    if (!button) return;

    applyBootstrapVisibility(button);
    if (bootstrapAvailable !== null || bootstrapProbeInFlight) return;
    bootstrapProbeInFlight = true;
    try {
      // Safe capability probe: the firmware checks the one-time gate before it
      // validates bootstrap fields. An empty bootstrap body can therefore tell
      // us whether the controller is factory-fresh without creating a user.
      const response = await fetch("/api/v1/access/users", {
        method: "POST",
        cache: "no-store",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ action: "bootstrap" })
      });
      const text = await response.text();
      let body = {};
      try { body = text ? JSON.parse(text) : {}; } catch (_) { body = {}; }

      if (response.status === 400 && body.reason === "invalid_bootstrap_admin") {
        bootstrapAvailable = true;
      } else if (response.status === 409 &&
                 (body.reason === "bootstrap_unavailable" || body.reason === "already_provisioned")) {
        bootstrapAvailable = false;
      }
    } catch (_) {
      bootstrapAvailable = null;
    } finally {
      bootstrapProbeInFlight = false;
      applyBootstrapVisibility(button);
    }

    if (button.dataset.hgBootstrapRefreshBound !== "1") {
      button.dataset.hgBootstrapRefreshBound = "1";
      button.addEventListener("click", () => {
        bootstrapAvailable = null;
        applyBootstrapVisibility(button);
        setTimeout(syncBootstrapAvailability, 900);
      }, true);
    }
  }

  function ensureFactoryResetUi() {
    if (!system || document.getElementById("firmwareFactoryResetPanel")) return;

    const panel = document.createElement("article");
    panel.id = "firmwareFactoryResetPanel";
    panel.className = "panel hg-factory-panel";
    panel.innerHTML = `
      <h3>Повне заводське скидання</h3>
      <p><small>Видаляє користувачів, Wi-Fi, Cloud та всі змінні налаштування. Прошивка і hardware identity залишаються.</small></p>
      <div class="hg-factory-fields">
        <label>Admin ID<input id="factoryActor" type="text" maxlength="23" autocomplete="username" placeholder="Admin ID"></label>
        <label>Admin PIN<input id="factoryCredential" type="password" inputmode="numeric" minlength="4" maxlength="12" autocomplete="current-password" placeholder="4–12 цифр"></label>
      </div>
      <div style="display:flex;gap:10px;align-items:center;flex-wrap:wrap;margin-top:14px">
        <button id="firmwareFactoryReset" type="button" class="hg-danger">Factory Reset</button>
        <span id="firmwareFactoryResetState">Потрібні Admin ID та PIN</span>
      </div>`;
    system.appendChild(panel);

    const actor = panel.querySelector("#factoryActor");
    const credential = panel.querySelector("#factoryCredential");
    const button = panel.querySelector("#firmwareFactoryReset");
    const state = panel.querySelector("#firmwareFactoryResetState");
    addSecretToggle(credential);

    button?.addEventListener("click", async () => {
      const actorValue = actor?.value.trim() || "";
      const credentialValue = credential?.value.trim() || "";
      if (!actorValue || !/^[0-9]{4,12}$/.test(credentialValue)) {
        if (state) state.textContent = "Введіть Admin ID та PIN 4–12 цифр";
        return;
      }

      if (!window.confirm("Factory Reset видалить користувачів, Wi-Fi, Cloud та всі змінні налаштування. Продовжити?")) return;
      if (!window.confirm("Підтвердьте ПОВНЕ СКИДАННЯ ще раз. Цю дію неможливо скасувати.")) return;

      button.disabled = true;
      if (state) state.textContent = "Виконується Factory Reset…";
      try {
        const response = await fetch("/api/v1/system/factory-reset", {
          method: "POST",
          cache: "no-store",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ actor: actorValue, credential: credentialValue, confirm: "ERASE_ALL" })
        });
        const text = await response.text();
        let body = {};
        try { body = text ? JSON.parse(text) : {}; } catch (_) { body = {}; }
        if (!response.ok || body.ok === false || body.rebooting !== true) {
          throw new Error(body.reason || `${response.status} ${response.statusText}`);
        }
        if (credential) credential.value = "";
        if (state) state.textContent = "Factory Reset прийнято. Контролер перезавантажується…";
      } catch (error) {
        if (state) state.textContent = `Factory Reset не виконано: ${error.message}`;
        button.disabled = false;
      }
    });
  }

  function dedupeFactoryResetControls() {
    const legacy = document.getElementById("factoryReset");
    if (legacy) {
      legacy.hidden = true;
      legacy.setAttribute("aria-hidden", "true");
      legacy.tabIndex = -1;
    }
  }

  function installWifiConnectHandoverFetchGuard() {
    if (window.__homeguardWifiConnectFetchGuard) return;
    const nativeFetch = window.fetch.bind(window);

    window.fetch = async (input, init = {}) => {
      const url = typeof input === "string" ? input : (input && typeof input.url === "string" ? input.url : "");
      const method = String(init.method || (input && input.method) || "GET").toUpperCase();
      const isWifiConnect = method === "POST" &&
        (url === "/api/v1/network/connect" || url.endsWith("/api/v1/network/connect"));

      try {
        return await nativeFetch(input, init);
      } catch (error) {
        if (!isWifiConnect) throw error;

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

  function enforceAcceptanceUi() {
    applyEmbeddedView();
    ensureFirmwareMobileNavigation();
    ensureFactoryResetUi();
    ensureSecretToggles();
    syncBootstrapAvailability();
    dedupeFactoryResetControls();
    enforceSingleActiveNav();
  }

  window.addEventListener("hashchange", () => {
    applyEmbeddedView();
    queueMicrotask(enforceSingleActiveNav);
    setTimeout(enforceSingleActiveNav, 0);
  });

  installWifiConnectHandoverFetchGuard();
  enforceAcceptanceUi();
  setInterval(enforceAcceptanceUi, 500);
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