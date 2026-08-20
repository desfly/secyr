"use strict";

(() => {
  const originalFetch = window.fetch.bind(window);
  let session = null;
  let gateMode = "loading";

  const style = document.createElement("style");
  style.textContent = `
    html.hg-auth-locked,html.hg-auth-locked body{margin:0;min-height:100%;background:#07131f;overflow:hidden}
    html.hg-auth-locked .shell{visibility:hidden!important;pointer-events:none!important}
    #hgAuthGate{position:fixed;inset:0;z-index:2147483000;display:grid;place-items:center;background:#07131f;overflow:auto}
    #hgAuthGate[hidden]{display:none!important}
    #hgAuthGate .hg-auth-stage{position:relative;width:100%;min-height:100vh;display:grid;place-items:center;padding:24px;box-sizing:border-box;overflow:hidden}
    #hgAuthGate .hg-auth-bruce{position:absolute;inset:0;width:100%;height:100%;object-fit:contain;object-position:center;opacity:.98;filter:none}
    #hgAuthGate .hg-auth-shade{position:absolute;inset:0;background:linear-gradient(180deg,rgba(3,12,20,.25),rgba(3,12,20,.58))}
    #hgAuthGate .hg-auth-card{position:relative;width:min(430px,calc(100vw - 32px));padding:22px;border:1px solid rgba(255,255,255,.28);border-radius:16px;background:rgba(7,19,31,.92);box-shadow:0 18px 60px rgba(0,0,0,.42);color:#fff;backdrop-filter:blur(8px)}
    #hgAuthGate.hg-setup-mode .hg-auth-card{width:min(900px,calc(100vw - 48px))}
    #hgAuthGate.hg-setup-mode .hg-auth-stage{overflow:auto}
    #hgAuthGate h2{margin:0 0 6px;font-size:24px}#hgAuthGate h3{margin:0 0 12px;font-size:18px}#hgAuthGate p{margin:0 0 16px;color:#c8d4df;line-height:1.4}
    #hgAuthGate label{display:block;margin:10px 0;font-weight:700}#hgAuthGate input,#hgAuthGate select{display:block;width:100%;margin-top:6px;padding:12px 13px;border:1px solid rgba(255,255,255,.28);border-radius:9px;background:#10243a;color:#fff;box-sizing:border-box;font:inherit}
    #hgAuthGate button{width:100%;margin-top:12px;padding:12px 14px;border:0;border-radius:9px;background:#fff;color:#10243a;font:inherit;font-weight:800;cursor:pointer}
    #hgAuthGate button.secondary{background:#18324c;color:#fff;border:1px solid rgba(255,255,255,.24)}
    #hgAuthGate button:disabled{opacity:.55;cursor:default}#hgAuthMessage{min-height:20px;margin-top:12px;color:#ffd2d2;font-size:14px}.hg-setup-status{min-height:18px;color:#c8d4df;font-size:13px;margin-top:8px}
    #hgAuthGate .hg-setup-grid{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:24px;align-items:start}
    #hgAuthGate .hg-setup-panel{min-width:0;padding:18px;border:1px solid rgba(255,255,255,.16);border-radius:12px;background:rgba(16,36,58,.48)}
    #hgAuthGate .hg-setup-networks{display:grid;gap:6px;max-height:230px;overflow:auto;margin-top:10px;padding-right:2px}
    #hgAuthGate .hg-setup-network{display:flex;align-items:center;justify-content:space-between;gap:12px;width:100%;margin:0;padding:10px 12px;text-align:left;background:#10243a;color:#fff;border:1px solid rgba(255,255,255,.18);border-radius:8px;font-weight:600}
    #hgAuthGate .hg-setup-network:hover,#hgAuthGate .hg-setup-network:focus-visible,#hgAuthGate .hg-setup-network[aria-selected="true"]{border-color:#6ea8ff;background:#183f68;outline:none}
    #hgAuthGate .hg-setup-network span{color:#b8c6d4;font-weight:500;white-space:nowrap}
    #hgSessionLogout{position:fixed;right:14px;bottom:14px;z-index:2000;padding:9px 12px;border-radius:9px;border:1px solid #d7deea;background:#fff;font:inherit;font-weight:700}
    .hg-legacy-auth-field{display:none!important}
    @media(min-width:1100px){
      #hgAuthGate.hg-setup-mode .hg-auth-stage{display:flex!important;align-items:center!important;justify-content:flex-start!important;padding:1.4vw!important;overflow:hidden!important}
      #hgAuthGate.hg-setup-mode .hg-auth-card{box-sizing:border-box!important;width:52vw!important;max-width:none!important;min-width:0!important;margin:0!important;padding:2vw!important;border-radius:18px!important;background:rgba(7,19,31,.98)!important;backdrop-filter:none!important}
      #hgAuthGate.hg-setup-mode h2{font-size:2vw!important;line-height:1.08!important;margin-bottom:.55vw!important}
      #hgAuthGate.hg-setup-mode h3{font-size:1.42vw!important;line-height:1.12!important;margin-bottom:.9vw!important}
      #hgAuthGate.hg-setup-mode p{font-size:.95vw!important;line-height:1.4!important;margin-bottom:1.15vw!important}
      #hgAuthGate.hg-setup-mode .hg-setup-grid{grid-template-columns:minmax(0,1fr) minmax(0,1fr)!important;gap:1.4vw!important}
      #hgAuthGate.hg-setup-mode .hg-setup-panel{padding:1.5vw!important;border-radius:14px!important}
      #hgAuthGate.hg-setup-mode label{font-size:1.05vw!important;line-height:1.2!important;margin:.75vw 0!important}
      #hgAuthGate.hg-setup-mode input,#hgAuthGate.hg-setup-mode select{min-height:3.25vw!important;font-size:1.05vw!important;line-height:1.2!important;padding:.8vw .95vw!important;border-radius:10px!important}
      #hgAuthGate.hg-setup-mode button{min-height:3.25vw!important;font-size:1.02vw!important;line-height:1.15!important;padding:.8vw .95vw!important;border-radius:10px!important}
      #hgAuthGate.hg-setup-mode .hg-setup-status{font-size:.9vw!important;line-height:1.35!important;margin-top:.65vw!important}
      #hgAuthGate.hg-setup-mode .hg-setup-networks{max-height:17vw!important;gap:.45vw!important}
      #hgAuthGate.hg-setup-mode .hg-setup-network{min-height:3vw!important;font-size:.95vw!important;padding:.65vw .8vw!important}
    }
    @media(max-width:720px){
      #hgAuthGate .hg-auth-stage{padding:16px}
      #hgAuthGate.hg-setup-mode .hg-auth-card{width:min(430px,calc(100vw - 24px));padding:18px}
      #hgAuthGate .hg-setup-grid{grid-template-columns:1fr;gap:14px}
      #hgAuthGate .hg-setup-panel{padding:14px}
      #hgAuthGate .hg-setup-networks{max-height:190px}
    }
  `;
  document.head.appendChild(style);
  document.documentElement.classList.add("hg-auth-locked");

  const gate = document.createElement("div");
  gate.id = "hgAuthGate";
  gate.innerHTML = `
    <div class="hg-auth-stage">
      <img class="hg-auth-bruce" src="/bruce.jpg?v=twofist-20260818" alt="" aria-hidden="true">
      <div class="hg-auth-shade"></div>
      <section class="hg-auth-card" aria-live="polite">
        <h2 id="hgAuthTitle">HomeGuard-S3</h2>
        <p id="hgAuthHint">Перевірка стану доступу…</p>
        <form id="hgAuthForm"></form>
        <div id="hgAuthMessage"></div>
      </section>
    </div>`;
  document.body.appendChild(gate);

  const form = gate.querySelector("#hgAuthForm");
  const title = gate.querySelector("#hgAuthTitle");
  const hint = gate.querySelector("#hgAuthHint");
  const message = gate.querySelector("#hgAuthMessage");

  function apiBody(response) {
    return response.text().then(text => {
      try { return text ? JSON.parse(text) : {}; } catch (_) { return {}; }
    });
  }

  function authHeader() { return session?.token ? `Bearer ${session.token}` : ""; }

  const bearerMutationRoutes = new Set([
    "/api/v1/system/security-command",
    "/api/v1/system/output-command",
    "/api/v1/system/factory-reset",
    "/api/v1/network/connect",
    "/api/v1/access/users",
    "/api/v1/cloud/config",
    "/api/v1/service/invalidate"
  ]);

  window.fetch = function(input, init = {}) {
    if (!session) return originalFetch(input, init);
    const url = typeof input === "string" ? input : String(input?.url || "");
    if (!url.startsWith("/api/v1/") || url === "/api/v1/access/login" || url === "/api/v1/access/state") return originalFetch(input, init);
    const headers = new Headers(init.headers || (typeof input !== "string" ? input.headers : undefined) || {});
    if (!headers.has("Authorization")) headers.set("Authorization", authHeader());

    let nextInit = { ...init, headers };
    if (bearerMutationRoutes.has(url) && typeof init.body === "string") {
      try {
        const payload = JSON.parse(init.body);
        if (payload && typeof payload === "object" && !Array.isArray(payload)) {
          payload.actor = session.actor;
          delete payload.credential;
          nextInit = { ...nextInit, body: JSON.stringify(payload) };
        }
      } catch (_) {
      }
    }

    return originalFetch(input, nextInit).then(response => {
      if (response.status === 401 && session) logout("Сеанс завершено. Увійдіть знову.");
      return response;
    });
  };

  function syncActorFields() {
    if (!session) return;
    ["#operatorId","#networkActor","#accessActor","#cloudActor","#factoryResetActor"].forEach(selector => {
      const actor=document.querySelector(selector); if(actor) actor.value=session.actor;
    });
  }

  function hideLegacyAuthUi() {
    ["#operatorId","#operatorPin","#networkActor","#networkCredential","#accessActor","#accessCredential","#cloudActor","#cloudCredential","#factoryResetActor","#factoryResetCredential"].forEach(selector => {
      const field=document.querySelector(selector);
      const label=field?.closest?.("label");
      (label || field)?.classList?.add("hg-legacy-auth-field");
    });
  }

  function primeLegacyHandlers() {
    if (!session) return;
    syncActorFields();
    ["#operatorPin","#networkCredential","#accessCredential","#cloudCredential","#factoryResetCredential"].forEach(selector => {
      const field=document.querySelector(selector); if(field) field.value="0000";
    });
    hideLegacyAuthUi();
  }

  function commandAllowed(command) {
    if (!session) return false;
    if (session.role === "admin") return true;
    const caps = session.capabilities || {};
    if (command === "security.arm_home") return caps.armHome === true;
    if (command === "security.arm_away") return caps.armAway === true;
    if (command === "security.disarm") return caps.disarm === true;
    if (command === "security.panic") return caps.panic === true;
    return false;
  }

  function applyRoleUi() {
    if (!session) return;
    const caps = session.capabilities || {};
    document.querySelectorAll("[data-command]").forEach(button => { button.disabled = !commandAllowed(button.dataset.command || ""); });
    document.querySelectorAll("[data-output-id]").forEach(button => { button.disabled = caps.valves !== true; });
    const wifiConnect = document.querySelector("#wifiConnect"); if (wifiConnect) wifiConnect.disabled = caps.networkConfigure !== true;
    ["#accessLoad", "#accessSave"].forEach(selector => { const button=document.querySelector(selector); if(button) button.disabled=caps.accessManage!==true; });
    primeLegacyHandlers();
  }

  function showLogin(prefill = "") {
    gate.classList.remove("hg-setup-mode");
    gateMode = "login_required"; title.textContent = "Вхід";
    hint.textContent = "Введіть ім’я користувача / ID та пароль PIN."; message.textContent = "";
    form.innerHTML = `<label>Користувач<input id="hgLoginActor" type="text" maxlength="23" autocomplete="username" value="${prefill.replace(/[&<>\"]/g, "")}"></label><label>Пароль / PIN<input id="hgLoginPin" type="password" inputmode="numeric" minlength="4" maxlength="12" autocomplete="current-password"></label><button type="submit">Увійти</button>`;
    form.querySelector("input")?.focus();
  }

  function showSetup() {
    gate.classList.add("hg-setup-mode");
    gateMode = "setup_required"; title.textContent = "Первинне налаштування";
    hint.textContent = "До створення першого Admin цей екран можна відкривати необмежену кількість разів. Налаштуйте Wi‑Fi за потреби, потім створіть адміністратора.";
    message.textContent = "";
    form.innerHTML = `
      <div class="hg-setup-grid">
        <section class="hg-setup-panel">
          <h3>1. Мережа Wi‑Fi</h3>
          <button id="hgSetupWifiScan" class="secondary" type="button">Сканувати Wi‑Fi</button>
          <div id="hgSetupWifiNetworks" class="hg-setup-networks" role="listbox" aria-label="Знайдені Wi-Fi мережі"></div>
          <label>SSID<input id="hgSetupWifiSsid" type="text" maxlength="32" autocomplete="off" placeholder="Оберіть мережу або введіть SSID"></label>
          <label>Пароль Wi‑Fi<input id="hgSetupWifiPassword" type="password" maxlength="64" autocomplete="current-password"></label>
          <button id="hgSetupWifiConnect" class="secondary" type="button">Підключити Wi‑Fi</button>
          <div id="hgSetupWifiStatus" class="hg-setup-status">Мережу можна налаштувати зараз або повернутися сюди пізніше до створення Admin.</div>
        </section>
        <section class="hg-setup-panel">
          <h3>2. Перший адміністратор</h3>
          <label>ID адміністратора<input id="hgSetupId" type="text" maxlength="23" autocomplete="username"></label>
          <label>Ім’я<input id="hgSetupName" type="text" maxlength="31" autocomplete="name"></label>
          <label>Пароль / PIN<input id="hgSetupPin" type="password" inputmode="numeric" minlength="4" maxlength="12" autocomplete="new-password"></label>
          <button type="submit">Створити Admin і закрити setup</button>
        </section>
      </div>`;
    form.querySelector("#hgSetupWifiScan")?.focus();
  }

  async function setupScanWifi() {
    const button=form.querySelector("#hgSetupWifiScan"),list=form.querySelector("#hgSetupWifiNetworks"),ssidField=form.querySelector("#hgSetupWifiSsid"),status=form.querySelector("#hgSetupWifiStatus");
    if(!button||!list||!ssidField||!status)return; button.disabled=true; status.textContent="Сканування…"; list.replaceChildren();
    try {
      const response=await originalFetch("/api/v1/network/scan",{cache:"no-store"}); const body=await apiBody(response);
      if(!response.ok||body.ok===false)throw new Error(body.reason||String(response.status));
      const networks=Array.isArray(body.networks)?body.networks:[];
      networks.forEach(item=>{
        const ssid=String(item.ssid||"");
        const row=document.createElement("button"); row.type="button"; row.className="hg-setup-network"; row.setAttribute("role","option"); row.setAttribute("aria-selected","false");
        const name=document.createElement("strong"); name.textContent=ssid||"(прихована мережа)";
        const signal=document.createElement("span"); signal.textContent=`${Number(item.rssi)||0} dBm`;
        row.append(name,signal);
        row.onclick=()=>{
          list.querySelectorAll(".hg-setup-network").forEach(candidate=>candidate.setAttribute("aria-selected","false"));
          row.setAttribute("aria-selected","true");
          if(ssid){ssidField.value=ssid;form.querySelector("#hgSetupWifiPassword")?.focus();}
          else{ssidField.value="";ssidField.focus();}
        };
        list.appendChild(row);
      });
      status.textContent=networks.length?`Знайдено мереж: ${networks.length}. Оберіть мережу зі списку.`:"Мереж не знайдено. SSID можна ввести вручну.";
      if(networks.length)list.querySelector(".hg-setup-network")?.focus();
      else ssidField.focus();
    } catch(error){status.textContent=`Помилка сканування: ${error.message}`;} finally {button.disabled=false;}
  }

  async function setupConnectWifi() {
    const button=form.querySelector("#hgSetupWifiConnect"),ssidField=form.querySelector("#hgSetupWifiSsid"),passwordField=form.querySelector("#hgSetupWifiPassword"),status=form.querySelector("#hgSetupWifiStatus");
    if(!button||!ssidField||!passwordField||!status)return; const ssid=ssidField.value.trim(),password=passwordField.value;
    if(!ssid){status.textContent="Оберіть Wi‑Fi мережу або введіть SSID.";return;} if(password && password.length<8){status.textContent="Пароль Wi‑Fi має містити щонайменше 8 символів.";return;}
    button.disabled=true; status.textContent="Підключення…";
    try {
      const response=await originalFetch("/api/v1/network/connect",{method:"POST",cache:"no-store",headers:{"Content-Type":"application/json"},body:JSON.stringify({ssid,password})});
      const body=await apiBody(response); if(!response.ok||body.ok===false)throw new Error(body.reason||String(response.status));
      passwordField.value=""; status.textContent=`Налаштування ${ssid} збережено. Контролер підключається; setup залишається відкритим до створення Admin.`;
    } catch(error){status.textContent=`Помилка підключення: ${error.message}`;} finally {passwordField.value="";button.disabled=false;}
  }

  async function loadAccessState() {
    try {
      const response=await originalFetch("/api/v1/access/state",{cache:"no-store"}); const body=await apiBody(response);
      if(!response.ok||body.ok===false)throw new Error(body.reason||String(response.status)); if(body.state==="setup_required")showSetup();else showLogin();
    } catch(error){title.textContent="HomeGuard-S3";hint.textContent="Стан доступу недоступний.";message.textContent=`Помилка: ${error.message}`;form.innerHTML='<button type="button" id="hgRetryAuth">Повторити</button>';form.querySelector("button").onclick=loadAccessState;}
  }

  async function submitSetup() {
    const id=form.querySelector("#hgSetupId")?.value.trim()||"",name=form.querySelector("#hgSetupName")?.value.trim()||"",pin=form.querySelector("#hgSetupPin")?.value.trim()||"";
    if(!id||!name||!/^\d{4,12}$/.test(pin)){message.textContent="Введіть ID, ім’я та PIN 4–12 цифр.";return;}
    const button=form.querySelector('button[type="submit"]');button.disabled=true;message.textContent="Створення адміністратора…";
    try {const response=await originalFetch("/api/v1/access/users",{method:"POST",cache:"no-store",headers:{"Content-Type":"application/json"},body:JSON.stringify({action:"bootstrap",id,name,pin})});const body=await apiBody(response);if(!response.ok||body.ok===false)throw new Error(body.reason||String(response.status));showLogin(id);message.textContent="Admin створений. Безпарольний setup закрито. Увійдіть новим паролем.";} catch(error){message.textContent=`Не вдалося створити Admin: ${error.message}`;button.disabled=false;}
  }

  async function submitLogin() {
    const actor=form.querySelector("#hgLoginActor")?.value.trim()||"";
    let credential=form.querySelector("#hgLoginPin")?.value.trim()||"";
    if(!actor||!/^\d{4,12}$/.test(credential)){message.textContent="Введіть користувача та PIN 4–12 цифр.";return;}
    const button=form.querySelector("button");button.disabled=true;message.textContent="Перевірка…";
    try {const response=await originalFetch("/api/v1/access/login",{method:"POST",cache:"no-store",headers:{"Content-Type":"application/json"},body:JSON.stringify({actor,credential})});const body=await apiBody(response);if(!response.ok||body.ok===false)throw new Error(body.reason||String(response.status));const token=String(body.sessionToken||"");if(!/^[0-9a-f]{64}$/.test(token))throw new Error("session_unavailable");form.querySelector("#hgLoginPin").value="";credential="";session={actor:String(body.actor||actor),token,name:String(body.name||actor),role:String(body.role||"guest"),capabilities:body.capabilities||{}};document.documentElement.classList.remove("hg-auth-locked");gate.hidden=true;syncActorFields();applyRoleUi();if(typeof refresh==="function")await refresh();applyRoleUi();ensureLogoutButton();}
    catch(error){credential="";session=null;message.textContent=error.message==="setup_required"?"Спочатку виконайте первинне налаштування.":`Вхід відхилено: ${error.message}`;if(error.message==="setup_required")showSetup();else button.disabled=false;}
  }

  function logout(reason="") {
    session=null;["#operatorId","#operatorPin","#networkActor","#networkCredential","#accessActor","#accessCredential","#cloudActor","#cloudCredential","#factoryResetActor","#factoryResetCredential"].forEach(selector=>{const field=document.querySelector(selector);if(field)field.value="";});
    document.querySelector("#hgSessionLogout")?.remove();gate.hidden=false;document.documentElement.classList.add("hg-auth-locked");showLogin();if(reason)message.textContent=reason;
  }

  function ensureLogoutButton(){if(document.querySelector("#hgSessionLogout"))return;const button=document.createElement("button");button.id="hgSessionLogout";button.type="button";button.textContent="Вийти";button.onclick=()=>logout();document.body.appendChild(button);}

  form.addEventListener("submit",event=>{event.preventDefault();if(gateMode==="setup_required")submitSetup();else submitLogin();});
  form.addEventListener("click",event=>{if(gateMode!=="setup_required")return;if(event.target?.id==="hgSetupWifiScan"){event.preventDefault();setupScanWifi();}else if(event.target?.id==="hgSetupWifiConnect"){event.preventDefault();setupConnectWifi();}});

  document.addEventListener("click",event=>{if(!session)return;const control=event.target.closest?.("[data-command],[data-output-id],#wifiConnect,#accessLoad,#accessSave,#cloudApply,#cloudDisable");if(!control)return;const caps=session.capabilities||{};let allowed=true;if(control.matches("[data-command]"))allowed=commandAllowed(control.dataset.command||"");else if(control.matches("[data-output-id]"))allowed=caps.valves===true;else if(control.matches("#wifiConnect"))allowed=caps.networkConfigure===true;else if(control.matches("#accessLoad,#accessSave"))allowed=caps.accessManage===true;else if(control.matches("#cloudApply,#cloudDisable"))allowed=session.role==="admin";if(!allowed){event.preventDefault();event.stopImmediatePropagation();if(typeof showToast==="function")showToast("Команда недоступна для цієї ролі");return;}primeLegacyHandlers();},true);

  const legacyObserver=new MutationObserver(()=>{if(session)applyRoleUi();});
  legacyObserver.observe(document.body,{childList:true,subtree:true});

  window.HomeGuardAuth={authenticated:()=>Boolean(session),actor:()=>session?.actor||"",role:()=>session?.role||"",logout};
  loadAccessState();
})();