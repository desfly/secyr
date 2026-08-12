"use strict";

(() => {
  let session = null;
  let mutationQueued = false;

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

  function applyRoleUi() {
    ensureSessionUi();
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
    return false;
  }

  // Backend authorization is still authoritative. The capture boundary also
  // blocks stale/transient UI states (for example app.js re-enabling buttons
  // in finally blocks) so a forbidden command cannot even be dispatched by
  // the browser between role-refresh cycles.
  document.addEventListener("click", event => {
    const protectedControl = event.target.closest?.("[data-command],[data-output-id],#wifiConnect,#accessLoad,#accessSave");
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

  const observer = new MutationObserver(() => {
    if (mutationQueued) return;
    mutationQueued = true;
    queueMicrotask(() => {
      mutationQueued = false;
      applyRoleUi();
    });
  });
  observer.observe(document.body, { childList: true, subtree: true });

  ensureSessionUi();
  applyRoleUi();
  setInterval(applyRoleUi, 1000);
})();
