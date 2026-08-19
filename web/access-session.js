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
    #hgAuthGate h2{margin:0 0 6px;font-size:24px}#hgAuthGate h3{margin:18px 0 8px;font-size:16px}#hgAuthGate p{margin:0 0 16px;color:#c8d4df;line-height:1.4}
    #hgAuthGate label{display:block;margin:10px 0;font-weight:700}#hgAuthGate input,#hgAuthGate select{display:block;width:100%;margin-top:6px;padding:12px 13px;border:1px solid rgba(255,255,255,.28);border-radius:9px;background:#10243a;color:#fff;box-sizing:border-box;font:inherit}
    #hgAuthGate button{width:100%;margin-top:12px;padding:12px 14px;border:0;border-radius:9px;background:#fff;color:#10243a;font:inherit;font-weight:800;cursor:pointer}
    #hgAuthGate button.secondary{background:#18324c;color:#fff;border:1px solid rgba(255,255,255,.24)}
    #hgAuthGate button:disabled{opacity:.55;cursor:default}#hgAuthMessage{min-height:20px;margin-top:12px;color:#ffd2d2;font-size:14px}.hg-setup-status{min-height:18px;color:#c8d4df;font-size:13px;margin-top:8px}
    #hgSessionLogout{position:fixed;right:14px;bottom:14px;z-index:2000;padding:9px 12px;border-radius:9px;border:1px solid #d7deea;background:#fff;font:inherit;font-weight:700}
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

  window.fetch = function(input, init = {}) {
    if (!session) return originalFetch(input, init);
    const url = typeof input === "string" ? input : String(input?.url || "");
    if (!url.startsWith("/api/v1/") || url === "/api/v1/access/login" || url === "/api/v1/access/state") return originalFetch(input, init);
    const headers = new Headers(init.headers || (typeof input !== "string" ? input.headers : undefined) || {});
    if (!headers.has("Authorization")) headers.set("Authorization", authHeader());
    return originalFetch(input, { ...init, headers }).then(response => {
      if (response.status === 401 && session) logout("Сеанс завершено. Увійдіть знову.");
      return response;
    });
  };

  function syncLegacyCredentials() {
    if (!session) return;
    [["#operatorId","#operatorPin"],["#networkActor","#networkCredential"],["#accessActor","#accessCredential"],["#cloudActor","#cloudCredential"]].forEach(([a,p]) => {
      const actor=document.querySelector(a),pin=document.querySelector(p); if(actor) actor.value=session.actor; if(pin) pin.value=session.credential;
    });
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
  }

  function showLogin(prefill = "") {
    gateMode = "login_required"; title.textContent = "Вхід";
    hint.textContent = "Введіть ім’я користувача / ID та пароль PIN."; message.textContent = "";
    form.innerHTML = `<label>Користувач<input id="hgLoginActor" type="text" maxlength="23" autocomplete="username" value="${prefill.replace(/[&<>\"]/g, "")}"></label><label>Пароль / PIN<input id="hgLoginPin" type="password" inputmode="numeric" minlength="4" maxlength="12" autocomplete="current-password"></label><button type="submit">Увійти</button>`;
    form.querySelector("input")?.focus();
  }

  function showSetup() {
    gateMode = "setup_required"; title.textContent = "Первинне налаштування";
    hint.textContent = "До створення першого Admin цей екран можна відкривати необмежену кількість разів. Налаштуйте Wi‑Fi за потреби, потім створіть адміністратора.";
    message.textContent = "";
    form.innerHTML = `
      <h3>1. Мережа Wi‑Fi</h3>
      <button id="hgSetupWifiScan" class="secondary" type="button">Сканувати Wi‑Fi</button>
      <label>Мережа<select id="hgSetupWifiSsid"><option value="">Оберіть мережу</option></select></label>
      <label>Пароль Wi‑Fi<input id="hgSetupWifiPassword" type="password" maxlength="64" autocomplete="current-password"></label>
      <button id="hgSetupWifiConnect" class="secondary" type="button">Підключити Wi‑Fi</button>
      <div id="hgSetupWifiStatus" class="hg-setup-status">Мережу можна налаштувати зараз або повернутися сюди пізніше до створення Admin.</div>
      <h3>2. Перший адміністратор</h3>
      <label>ID адміністратора<input id="hgSetupId" type="text" maxlength="23" autocomplete="username"></label>
      <label>Ім’я<input id="hgSetupName" type="text" maxlength="31" autocomplete="name"></label>
      <label>Пароль / PIN<input id="hgSetupPin" type="password" inputmode="numeric" minlength="4" maxlength="12" autocomplete="new-password"></label>
      <button type="submit">Створити Admin і закрити setup</button>`;
    form.querySelector("#hgSetupId")?.focus();
  }

  async function setupScanWifi() {
    const button=form.querySelector("#hgSetupWifiScan"),select=form.querySelector("#hgSetupWifiSsid"),status=form.querySelector("#hgSetupWifiStatus");
    if(!button||!select||!status)return; button.disabled=true; status.textContent="Сканування…";
    try {
      const response=await originalFetch("/api/v1/network/scan",{cache:"no-store"}); const body=await apiBody(response);
      if(!response.ok||body.ok===false)throw new Error(body.reason||String(response.status));
      const networks=Array.isArray(body.networks)?body.networks:[]; select.innerHTML='<option value="">Оберіть мережу</option>';
      networks.forEach(item=>{const option=document.createElement("option");option.value=String(item.ssid||"");option.textContent=`${item.ssid||"(прихована)"} · ${Number(item.rssi)||0} dBm`;select.appendChild(option);});
      status.textContent=networks.length?`Знайдено мереж: ${networks.length}`:"Мереж не знайдено";
    } catch(error){status.textContent=`Помилка сканування: ${error.message}`;} finally {button.disabled=false;}
  }

  async function setupConnectWifi() {
    const button=form.querySelector("#hgSetupWifiConnect"),select=form.querySelector("#hgSetupWifiSsid"),passwordField=form.querySelector("#hgSetupWifiPassword"),status=form.querySelector("#hgSetupWifiStatus");
    if(!button||!select||!passwordField||!status)return; const ssid=select.value.trim(),password=passwordField.value;
    if(!ssid){status.textContent="Оберіть Wi‑Fi мережу.";return;} if(password && password.length<8){status.textContent="Пароль Wi‑Fi має містити щонайменше 8 символів.";return;}
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
    const actor=form.querySelector("#hgLoginActor")?.value.trim()||"",credential=form.querySelector("#hgLoginPin")?.value.trim()||"";
    if(!actor||!/^\d{4,12}$/.test(credential)){message.textContent="Введіть користувача та PIN 4–12 цифр.";return;}
    const button=form.querySelector("button");button.disabled=true;message.textContent="Перевірка…";
    try {const response=await originalFetch("/api/v1/access/login",{method:"POST",cache:"no-store",headers:{"Content-Type":"application/json"},body:JSON.stringify({actor,credential})});const body=await apiBody(response);if(!response.ok||body.ok===false)throw new Error(body.reason||String(response.status));const token=String(body.sessionToken||"");if(!/^[0-9a-f]{64}$/.test(token))throw new Error("session_unavailable");session={actor:String(body.actor||actor),credential,token,name:String(body.name||actor),role:String(body.role||"guest"),capabilities:body.capabilities||{}};document.documentElement.classList.remove("hg-auth-locked");gate.hidden=true;syncLegacyCredentials();applyRoleUi();if(typeof refresh==="function")await refresh();ensureLogoutButton();}
    catch(error){session=null;message.textContent=error.message==="setup_required"?"Спочатку виконайте первинне налаштування.":`Вхід відхилено: ${error.message}`;if(error.message==="setup_required")showSetup();else button.disabled=false;}
  }

  function logout(reason="") {
    session=null;["#operatorId","#operatorPin","#networkActor","#networkCredential","#accessActor","#accessCredential","#cloudActor","#cloudCredential"].forEach(selector=>{const field=document.querySelector(selector);if(field)field.value="";});
    document.querySelector("#hgSessionLogout")?.remove();gate.hidden=false;document.documentElement.classList.add("hg-auth-locked");showLogin();if(reason)message.textContent=reason;
  }

  function ensureLogoutButton(){if(document.querySelector("#hgSessionLogout"))return;const button=document.createElement("button");button.id="hgSessionLogout";button.type="button";button.textContent="Вийти";button.onclick=()=>logout();document.body.appendChild(button);}

  form.addEventListener("submit",event=>{event.preventDefault();if(gateMode==="setup_required")submitSetup();else submitLogin();});
  form.addEventListener("click",event=>{if(gateMode!=="setup_required")return;if(event.target?.id==="hgSetupWifiScan"){event.preventDefault();setupScanWifi();}else if(event.target?.id==="hgSetupWifiConnect"){event.preventDefault();setupConnectWifi();}});

  document.addEventListener("click",event=>{if(!session)return;const control=event.target.closest?.("[data-command],[data-output-id],#wifiConnect,#accessLoad,#accessSave,#cloudApply,#cloudDisable");if(!control)return;const caps=session.capabilities||{};let allowed=true;if(control.matches("[data-command]"))allowed=commandAllowed(control.dataset.command||"");else if(control.matches("[data-output-id]"))allowed=caps.valves===true;else if(control.matches("#wifiConnect"))allowed=caps.networkConfigure===true;else if(control.matches("#accessLoad,#accessSave"))allowed=caps.accessManage===true;else if(control.matches("#cloudApply,#cloudDisable"))allowed=session.role==="admin";if(!allowed){event.preventDefault();event.stopImmediatePropagation();if(typeof showToast==="function")showToast("Команда недоступна для цієї ролі");return;}syncLegacyCredentials();},true);

  window.HomeGuardAuth={authenticated:()=>Boolean(session),actor:()=>session?.actor||"",role:()=>session?.role||"",logout};
  loadAccessState();
})();
