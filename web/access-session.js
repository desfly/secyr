"use strict";

(() => {
  let session = null;
  let mutationQueued = false;
  let lastSidebarLink = null;

  const roleLabel = role => ({ admin: "Admin", user: "User", guest: "Guest" })[role] || role || "—";
  const setText = (element, value) => {
    if (element && element.textContent !== value) element.textContent = value;
  };

  async function loginApi(actor, credential) {
    const response = await fetch("/api/v1/access/login", {
      method: "POST",
      cache: "no-store",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ actor, credential })
    });
    const text = await response.text();
    let body = {};
    try { body = text ? JSON.parse(text) : {}; } catch (_) { body = {}; }
    if (!response.ok || body.ok === false) throw new Error(body.reason || `${response.status} ${response.statusText}`);
    return body;
  }

  function ensureSessionUi() {
    if (document.querySelector("#accessSessionBar")) return;
    const operatorId = document.querySelector("#operatorId");
    const quickPanel = operatorId?.closest("article.panel");
    const heading = quickPanel?.querySelector("h3");
    if (!quickPanel || !heading) return;

    const bar = document.createElement("div");
    bar.id = "accessSessionBar";
    bar.style.cssText = "display:flex;gap:10px;align-items:center;flex-wrap:wrap;margin:0 0 12px;padding:10px 12px;border:1px solid #d7deea;border-radius:9px";
    bar.innerHTML = `
      <strong id="accessSessionState">Не авторизовано</strong>
      <span id="accessSessionRole" style="margin-right:auto">Увійдіть для керування</span>
      <button id="accessLogin" type="button">Увійти</button>
      <button id="accessLogout" type="button" hidden>Вийти</button>`;
    heading.insertAdjacentElement("afterend", bar);

    document.querySelector("#accessLogin").addEventListener("click", login);
    document.querySelector("#accessLogout").addEventListener("click", logout);
    document.querySelector("#operatorPin")?.addEventListener("keydown", event => {
      if (event.key === "Enter") {
        event.preventDefault();
        login();
      }
    });
  }

  function ensureMobileNavigation() {
    const sidebar = document.querySelector(".sidebar");
    const bruce = document.querySelector(".sidebar .bruce");
    const nav = document.querySelector(".sidebar nav");
    if (!sidebar || !bruce || !nav) return;

    if (!document.querySelector("#homeguardMobileNavStyle")) {
      const style = document.createElement("style");
      style.id = "homeguardMobileNavStyle";
      style.textContent = `
        .mobile-menu-toggle{display:none}
        @media (max-width:760px){
          .sidebar{position:relative!important;height:auto!important;min-height:0!important;overflow:visible!important}
          .sidebar .brand{justify-content:center!important}
          .sidebar .bruce{height:150px!important;margin:4px 10px 8px!important;overflow:hidden!important}
          .sidebar .bruce img{width:100%!important;height:100%!important;object-fit:contain!important;object-position:center center!important}
          .mobile-menu-toggle{display:flex!important;width:100%!important;min-height:44px!important;margin:0 0 8px!important;padding:10px 14px!important;align-items:center!important;justify-content:space-between!important;border:1px solid rgba(255,255,255,.22)!important;border-radius:10px!important;background:rgba(255,255,255,.08)!important;color:#fff!important;font:inherit!important;font-weight:700!important}
          .sidebar nav{display:none!important;position:static!important;width:100%!important;max-height:none!important;overflow:visible!important;margin:0!important;padding:0!important;z-index:auto!important}
          .sidebar.mobile-menu-open nav{display:grid!important;grid-template-columns:repeat(2,minmax(0,1fr))!important;gap:6px!important}
          .sidebar nav a{min-width:0!important}
          .sidebar .side-foot{display:none!important}
        }
        @media (max-width:430px){
          .sidebar.mobile-menu-open nav{grid-template-columns:1fr!important}
          .sidebar .bruce{height:132px!important}
        }
      `;
      document.head.appendChild(style);
    }

    if (!document.querySelector("#mobileMenuToggle")) {
      const toggle = document.createElement("button");
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
        toggle.lastElementChild.textContent = open ? "⌃" : "⌄";
      });
    }

    nav.querySelectorAll("a").forEach(link => {
      if (link.dataset.mobileCloseBound === "1") return;
      link.dataset.mobileCloseBound = "1";
      link.addEventListener("click", () => {
        if (window.matchMedia("(max-width:760px)").matches) {
          sidebar.classList.remove("mobile-menu-open");
          const toggle = document.querySelector("#mobileMenuToggle");
          if (toggle) {
            toggle.setAttribute("aria-expanded", "false");
            if (toggle.lastElementChild) toggle.lastElementChild.textContent = "⌄";
          }
        }
      });
    });

    window.addEventListener("resize", () => {
      if (!window.matchMedia("(max-width:760px)").matches) {
        sidebar.classList.remove("mobile-menu-open");
        const toggle = document.querySelector("#mobileMenuToggle");
        if (toggle) toggle.setAttribute("aria-expanded", "false");
      }
    }, { passive: true });
  }

  function commandAllowed(command) {
    if (!session) return false;
    const caps = session.capabilities || {};
    if (session.role === "admin") return true;
    if (command === "security.arm_home") return caps.armHome === true;
    if (command === "security.arm_away") return caps.armAway === true;
    if (command === "security.disarm") return caps.disarm === true;
    if (command === "security.panic") return caps.panic === true;
    return false;
  }

  function syncCredentialFields() {
    if (!session) return;
    const pairs = [
      ["#operatorId", "#operatorPin"],
      ["#networkActor", "#networkCredential"],
      ["#accessActor", "#accessCredential"]
    ];
    pairs.forEach(([actorSelector, pinSelector]) => {
      const actor = document.querySelector(actorSelector);
      const pin = document.querySelector(pinSelector);
      if (actor) actor.value = session.actor;
      if (pin) pin.value = session.credential;
    });
  }

  function adminSession() {
    return session && session.role === "admin" ? session : null;
  }

  async function parseApiResponse(response) {
    const text = await response.text();
    let body = {};
    try { body = text ? JSON.parse(text) : {}; } catch (_) { body = {}; }
    if (!response.ok || body.ok === false) throw new Error(body.reason || `${response.status} ${response.statusText}`);
    return body;
  }

  async function exportConfig() {
    const admin = adminSession();
    if (!admin) {
      if (typeof showToast === "function") showToast("Export конфігурації доступний тільки Admin");
      return;
    }
    if (!window.confirm("Експорт міститиме Wi-Fi та Cloud паролі. Створити повну резервну копію?")) return;

    const button = document.querySelector("#configExport");
    if (button) button.disabled = true;
    try {
      const response = await fetch("/api/v1/config/export", {
        method: "POST",
        cache: "no-store",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ actor: admin.actor, credential: admin.credential, confirm: "INCLUDE_SECRETS" })
      });
      const text = await response.text();
      if (!response.ok) {
        let errorBody = {};
        try { errorBody = text ? JSON.parse(text) : {}; } catch (_) { errorBody = {}; }
        throw new Error(errorBody.reason || `${response.status} ${response.statusText}`);
      }
      const backup = JSON.parse(text);
      if (backup.format !== "homeguard-config" || backup.version !== 1 || backup.secretsIncluded !== true) {
        throw new Error("invalid_export_format");
      }
      const blob = new Blob([text], { type: "application/json" });
      const url = URL.createObjectURL(blob);
      const link = document.createElement("a");
      link.href = url;
      link.download = "homeguard-config-v1.json";
      document.body.appendChild(link);
      link.click();
      link.remove();
      URL.revokeObjectURL(url);
      if (typeof showToast === "function") showToast("Резервну копію конфігурації створено");
    } catch (error) {
      if (typeof showToast === "function") showToast(`Export не виконано: ${error.message}`);
    } finally {
      applyRoleUi();
    }
  }

  async function importConfigFile(file) {
    const admin = adminSession();
    if (!admin || !file) return;
    if (file.size === 0 || file.size > 8192) {
      if (typeof showToast === "function") showToast("Файл конфігурації має неправильний розмір");
      return;
    }

    let backup;
    try {
      backup = JSON.parse(await file.text());
    } catch (_) {
      if (typeof showToast === "function") showToast("Файл не є коректним JSON");
      return;
    }
    if (backup?.format !== "homeguard-config" || backup?.version !== 1 || backup?.secretsIncluded !== true) {
      if (typeof showToast === "function") showToast("Цей backup не можна відновити: потрібен HomeGuard Config v1 із секретами");
      return;
    }
    if (!window.confirm("Імпорт замінить користувачів, Wi-Fi, Cloud і commissioning state та перезавантажить контролер. Продовжити?")) return;

    const button = document.querySelector("#configImport");
    if (button) button.disabled = true;
    try {
      const response = await fetch("/api/v1/config/import", {
        method: "POST",
        cache: "no-store",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ actor: admin.actor, credential: admin.credential, confirm: "APPLY_CONFIG", backup })
      });
      const body = await parseApiResponse(response);
      if (body.rebooting !== true) throw new Error("reboot_not_confirmed");
      if (typeof showToast === "function") showToast("Конфігурацію відновлено. Контролер перезавантажується…");
      session = null;
      applyRoleUi();
    } catch (error) {
      if (typeof showToast === "function") showToast(`Import не виконано: ${error.message}`);
      applyRoleUi();
    }
  }

  async function factoryReset() {
    const admin = adminSession();
    if (!admin) {
      if (typeof showToast === "function") showToast("Factory Reset доступний тільки Admin");
      return;
    }
    if (!window.confirm("Factory Reset видалить користувачів, Wi-Fi, Cloud та всі змінні налаштування. Firmware і hardware identity залишаться. Продовжити?")) return;
    if (!window.confirm("Підтвердьте ПОВНЕ СКИДАННЯ ще раз. Цю дію неможливо скасувати.")) return;

    const button = document.querySelector("#factoryReset");
    if (button) button.disabled = true;
    try {
      const response = await fetch("/api/v1/system/factory-reset", {
        method: "POST",
        cache: "no-store",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ actor: admin.actor, credential: admin.credential, confirm: "ERASE_ALL" })
      });
      const body = await parseApiResponse(response);
      if (body.rebooting !== true) throw new Error("reboot_not_confirmed");
      session = null;
      applyRoleUi();
      if (typeof showToast === "function") showToast("Factory Reset виконано. Контролер перезавантажується…");
    } catch (error) {
      if (typeof showToast === "function") showToast(`Factory Reset не виконано: ${error.message}`);
      applyRoleUi();
    }
  }

  function ensureConfigToolsUi() {
    if (document.querySelector("#configTools")) return;
    const system = document.querySelector("#system");
    if (!system) return;

    const panel = document.createElement("article");
    panel.id = "configTools";
    panel.className = "panel cloud-config";
    panel.innerHTML = `
      <h3>Резервна копія та повне скидання</h3>
      <p><small>Тільки Admin. Export містить секрети. Import застосовується транзакційно з rollback і перезавантаженням.</small></p>
      <div style="display:flex;gap:10px;align-items:center;flex-wrap:wrap;margin-top:14px">
        <button id="configExport" type="button">Export config</button>
        <button id="configImport" type="button">Import config</button>
        <button id="factoryReset" type="button">Factory Reset</button>
        <input id="configImportFile" type="file" accept="application/json,.json" hidden>
        <span id="configToolsState">Увійдіть як Admin</span>
      </div>`;
    system.appendChild(panel);

    panel.querySelector("#configExport")?.addEventListener("click", exportConfig);
    panel.querySelector("#configImport")?.addEventListener("click", () => {
      if (!adminSession()) return;
      const input = panel.querySelector("#configImportFile");
      if (input) {
        input.value = "";
        input.click();
      }
    });
    panel.querySelector("#configImportFile")?.addEventListener("change", event => {
      const file = event.target.files?.[0] || null;
      importConfigFile(file);
    });
    panel.querySelector("#factoryReset")?.addEventListener("click", factoryReset);
  }

  function applyRoleUi() {
    ensureSessionUi();
    ensureConfigToolsUi();
    const caps = session?.capabilities || {};
    const loggedIn = Boolean(session);
    const isAdmin = session?.role === "admin";

    const state = document.querySelector("#accessSessionState");
    const role = document.querySelector("#accessSessionRole");
    const loginButton = document.querySelector("#accessLogin");
    const logoutButton = document.querySelector("#accessLogout");
    const operatorId = document.querySelector("#operatorId");
    const operatorPin = document.querySelector("#operatorPin");

    setText(state, loggedIn ? (session.name || session.actor) : "Не авторизовано");
    setText(role, loggedIn ? `Роль: ${roleLabel(session.role)}` : "Увійдіть для керування");
    if (loginButton) loginButton.hidden = loggedIn;
    if (logoutButton) logoutButton.hidden = !loggedIn;
    if (operatorId) operatorId.readOnly = loggedIn;
    if (operatorPin) operatorPin.readOnly = loggedIn;

    document.querySelectorAll("[data-command]").forEach(button => {
      const allowed = commandAllowed(button.dataset.command || "");
      button.disabled = !allowed;
      button.title = allowed ? "" : (loggedIn ? "Недоступно для цієї ролі" : "Спочатку увійдіть");
    });

    document.querySelectorAll("[data-output-id]").forEach(button => {
      if (!loggedIn || caps.valves !== true) {
        button.disabled = true;
        button.title = loggedIn ? "Керування клапанами недоступне для цієї ролі" : "Спочатку увійдіть";
      }
    });

    const wifiConnect = document.querySelector("#wifiConnect");
    if (wifiConnect) {
      wifiConnect.disabled = !loggedIn || caps.networkConfigure !== true;
      wifiConnect.title = wifiConnect.disabled ? "Зміна Wi-Fi доступна тільки Admin" : "";
    }

    ["#accessLoad", "#accessSave"].forEach(selector => {
      const button = document.querySelector(selector);
      if (button) {
        button.disabled = !loggedIn || caps.accessManage !== true;
        button.title = button.disabled ? "Керування користувачами доступне тільки Admin" : "";
      }
    });

    ["#configExport", "#configImport", "#factoryReset"].forEach(selector => {
      const button = document.querySelector(selector);
      if (button) {
        button.disabled = !isAdmin;
        button.title = isAdmin ? "" : "Доступно тільки Admin";
      }
    });
    setText(document.querySelector("#configToolsState"), isAdmin ? "Admin · готово" : "Увійдіть як Admin");

    const networkAuth = document.querySelector("#networkAuth");
    if (networkAuth) networkAuth.hidden = loggedIn && isAdmin;

    const adminActor = document.querySelector("#accessActor")?.closest("label");
    const adminPin = document.querySelector("#accessCredential")?.closest("label");
    if (adminActor) adminActor.hidden = loggedIn && isAdmin;
    if (adminPin) adminPin.hidden = loggedIn && isAdmin;
  }

  async function login() {
    ensureSessionUi();
    const actor = document.querySelector("#operatorId")?.value.trim() || "";
    const credential = document.querySelector("#operatorPin")?.value.trim() || "";
    if (!actor || !/^\d{4,12}$/.test(credential)) {
      if (typeof showToast === "function") showToast("Введіть ID користувача та PIN 4–12 цифр");
      return;
    }

    const button = document.querySelector("#accessLogin");
    if (button) button.disabled = true;
    try {
      const result = await loginApi(actor, credential);
      session = {
        actor: String(result.actor || actor),
        credential,
        name: String(result.name || actor),
        role: String(result.role || "guest"),
        capabilities: result.capabilities || {}
      };
      document.querySelector("#operatorPin").value = "";
      applyRoleUi();
      if (typeof refresh === "function") await refresh();
      applyRoleUi();
      if (typeof showToast === "function") showToast(`Вхід виконано · ${roleLabel(session.role)}`);
    } catch (error) {
      session = null;
      document.querySelector("#operatorPin").value = "";
      applyRoleUi();
      if (typeof showToast === "function") showToast(`Вхід відхилено: ${error.message}`);
    } finally {
      if (button) button.disabled = false;
    }
  }

  function logout() {
    session = null;
    ["#operatorId", "#operatorPin", "#networkActor", "#networkCredential", "#accessActor", "#accessCredential"].forEach(selector => {
      const field = document.querySelector(selector);
      if (field) field.value = "";
    });
    applyRoleUi();
    if (typeof showToast === "function") showToast("Сеанс завершено");
  }

  function protectedActionAllowed(control) {
    if (!session || !control) return false;
    const caps = session.capabilities || {};
    if (control.matches("[data-command]")) return commandAllowed(control.dataset.command || "");
    if (control.matches("[data-output-id]")) return caps.valves === true;
    if (control.matches("#wifiConnect")) return caps.networkConfigure === true;
    if (control.matches("#accessLoad,#accessSave")) return caps.accessManage === true;
    if (control.matches("#configExport,#configImport,#factoryReset")) return session.role === "admin";
    return false;
  }

  function hideRawBuildInfo() {
    const buildInfo = document.querySelector("#buildInfo");
    if (buildInfo) buildInfo.hidden = true;
  }

  function enforceSingleSidebarActive() {
    const links = [...document.querySelectorAll(".sidebar nav a")];
    if (!links.length) return;
    const hash = window.location.hash || "#overview";
    const matching = links.filter(link => link.getAttribute("href") === hash);
    const activeLinks = links.filter(link => link.classList.contains("active"));
    let preferred = null;

    if (lastSidebarLink && lastSidebarLink.isConnected && lastSidebarLink.getAttribute("href") === hash) {
      preferred = lastSidebarLink;
    } else if (matching.length) {
      preferred = matching[0];
    } else if (activeLinks.length) {
      preferred = activeLinks[0];
    } else {
      preferred = links[0];
    }

    links.forEach(link => {
      const shouldBeActive = link === preferred;
      if (link.classList.contains("active") !== shouldBeActive) {
        link.classList.toggle("active", shouldBeActive);
      }
    });
  }

  document.addEventListener("click", event => {
    const protectedControl = event.target.closest?.("[data-command],[data-output-id],#wifiConnect,#accessLoad,#accessSave,#configExport,#configImport,#factoryReset");
    if (!protectedControl) return;
    if (!protectedActionAllowed(protectedControl)) {
      event.preventDefault();
      event.stopImmediatePropagation();
      applyRoleUi();
      if (typeof showToast === "function") {
        showToast(session ? "Команда недоступна для цієї ролі" : "Спочатку увійдіть");
      }
      return;
    }
    syncCredentialFields();
  }, true);

  document.addEventListener("click", event => {
    const network = event.target.closest?.(".wifi-network[data-ssid]");
    if (!network) return;
    const ssid = network.dataset.ssid || "";
    setTimeout(() => {
      const list = document.querySelector("#wifiNetworks");
      const state = document.querySelector("#scanState");
      if (list) list.innerHTML = "";
      if (state) state.textContent = ssid ? `Вибрано: ${ssid}` : "Вибрано приховану мережу";
    }, 0);
  });

  document.addEventListener("click", event => {
    const link = event.target.closest?.(".sidebar nav a");
    if (!link) return;
    lastSidebarLink = link;
    // Repair immediately in the click turn so duplicate href entries never
    // survive until the browser paints the next frame.
    enforceSingleSidebarActive();
    queueMicrotask(enforceSingleSidebarActive);
    setTimeout(enforceSingleSidebarActive, 0);
  });
  window.addEventListener("hashchange", () => {
    // app.js may mark every link sharing this hash active. Collapse it back to
    // the selected link synchronously before paint; async calls are fallback.
    enforceSingleSidebarActive();
    queueMicrotask(enforceSingleSidebarActive);
    setTimeout(enforceSingleSidebarActive, 0);
  });

  const observer = new MutationObserver(() => {
    if (mutationQueued) return;
    mutationQueued = true;
    queueMicrotask(() => {
      mutationQueued = false;
      applyRoleUi();
      hideRawBuildInfo();
      enforceSingleSidebarActive();
    });
  });
  observer.observe(document.body, { childList: true, subtree: true, attributes: true, attributeFilter: ["class"] });

  ensureSessionUi();
  ensureMobileNavigation();
  ensureConfigToolsUi();
  hideRawBuildInfo();
  applyRoleUi();
  enforceSingleSidebarActive();
  setInterval(() => {
    applyRoleUi();
    hideRawBuildInfo();
    enforceSingleSidebarActive();
  }, 1000);
})();
