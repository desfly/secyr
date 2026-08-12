"use strict";

const toast = document.querySelector("#toast");
const dashboardSections = [document.querySelector(".status-grid"), document.querySelector(".two-col")].filter(Boolean);
const networkPage = document.querySelector("#networkPage");
const systemPage = document.querySelector("#system");

function showToast(message) {
  toast.textContent = message;
  toast.hidden = false;
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => { toast.hidden = true; }, 3000);
}

function escapeHtml(value) {
  return String(value ?? "")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/\"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

async function api(path, options = {}) {
  const response = await fetch(path, {
    cache: "no-store",
    ...options,
    headers: { "Content-Type": "application/json", ...(options.headers || {}) }
  });
  const text = await response.text();
  let body = {};
  try { body = text ? JSON.parse(text) : {}; } catch (_) { body = { raw: text }; }
  if (!response.ok || body.ok === false) throw new Error(body.reason || `${response.status} ${response.statusText}`);
  return body;
}

function validPin(pin) { return /^\d{4,12}$/.test(pin); }
function stateClass(value) {
  if (["normal", "closed", "ready", "disarmed", "stay", "away"].includes(value)) return "ok";
  if (["alarm", "fault", "jammed", "timeout", "tamper"].includes(value)) return "alarm";
  return "warning";
}
function armLabel(value) { return ({ disarmed: "ЗНЯТО", stay: "НІЧНИЙ", away: "ПІД ОХОРОНОЮ", alarm: "ТРИВОГА" })[value] || "—"; }
function wifiStateLabel(value) { return ({ connected: "Підключено", connecting: "Підключення…", idle: "Не налаштовано", error: "Помилка" })[value] || "Перевірка…"; }

function renderZones(data) {
  const zones = Array.isArray(data?.zones) ? data.zones : [];
  document.querySelector("#zoneCount").textContent = zones.length || "—";
  document.querySelector("#zones").innerHTML = zones.length ? zones.map(zone => `
    <div class="zone"><span>${escapeHtml(zone.name || `Зона ${zone.id}`)}${zone.alwaysOn ? " · 24/7" : ""}</span><strong class="${stateClass(zone.state)}">${escapeHtml(zone.state)}</strong></div>`).join("") : "<div class=\"zone\"><span>Дані ще не отримані</span><strong>—</strong></div>";
}

function renderPartitions(data) {
  const partition = Array.isArray(data?.partitions) ? data.partitions[0] : null;
  document.querySelector("#securityMode").textContent = armLabel(partition?.armState);
}

function renderEvents(data) {
  const events = Array.isArray(data?.events) ? data.events.slice(-6).reverse() : [];
  document.querySelector("#eventList").innerHTML = events.length ? events.map(item => `
    <div><i></i><time>#${escapeHtml(item.sequence ?? "—")}</time><span>${escapeHtml(item.event || "Подія")}</span><a>${escapeHtml(item.severity || "info")}</a></div>`).join("") : "<div><i></i><time>—</time><span>Подій ще немає</span><a>Інформація ›</a></div>";
}

function renderOutputs(data) {
  const outputs = Array.isArray(data?.outputs) ? data.outputs : [];
  const target = document.querySelector("#ioState");
  target.innerHTML = outputs.length ? outputs.map(item => {
    const id = Number(item.id) || 0;
    const isValve = item.type === "valve";
    const controls = isValve ? `<span style="display:flex;gap:6px;margin-left:auto"><button type="button" data-output-id="${id}" data-output-active="true" ${item.active ? "disabled" : ""}>Відкрити</button><button type="button" data-output-id="${id}" data-output-active="false" ${item.active ? "" : "disabled"}>Закрити</button></span>` : "";
    return `<div class="${item.active ? "" : "muted"}"><b>⇆</b><span>${isValve ? "Клапан" : "Вих."} ${id}</span><small>${item.active ? "Увімк." : "Вимк."}</small>${controls}</div>`;
  }).join("") : "<div><span>Очікування реальних даних контролера…</span></div>";
  target.querySelectorAll("[data-output-id]").forEach(button => { button.onclick = () => sendOutputCommand(button); });
}

function renderNetwork(status) {
  const state = status?.state || "error";
  const ssid = status?.ssid || "—";
  const ip = status?.ip || "—";
  document.querySelector("#wifiName").textContent = ssid;
  document.querySelector("#connection").textContent = wifiStateLabel(state);
  document.querySelector("#networkState").textContent = wifiStateLabel(state);
  document.querySelector("#networkSsid").textContent = ssid;
  document.querySelector("#networkIp").textContent = ip;
  if (ssid !== "—" && !document.querySelector("#wifiSsid").value) document.querySelector("#wifiSsid").value = ssid;
}

async function refreshNetwork() {
  try {
    const status = await api("/api/v1/network/status");
    renderNetwork(status);
    return status;
  } catch (error) {
    renderNetwork({ state: "error", ssid: "—", ip: "—" });
    document.querySelector("#wifiResult").textContent = `Помилка: ${error.message}`;
    return null;
  }
}

async function refresh() {
  const requests = await Promise.allSettled([
    api("/api/v1/system/zones"), api("/api/v1/system/partitions"), api("/api/v1/system/outputs"),
    api("/api/v1/system/events"), api("/api/v1/build"), api("/api/v1/network/status")
  ]);
  if (requests[0].status === "fulfilled") renderZones(requests[0].value);
  if (requests[1].status === "fulfilled") renderPartitions(requests[1].value);
  if (requests[2].status === "fulfilled") renderOutputs(requests[2].value);
  if (requests[3].status === "fulfilled") renderEvents(requests[3].value);
  if (requests[4].status === "fulfilled") document.querySelector("#buildInfo").textContent = JSON.stringify(requests[4].value, null, 2);
  if (requests[5].status === "fulfilled") renderNetwork(requests[5].value);
}

async function scanWifi() {
  const button = document.querySelector("#wifiScan");
  const state = document.querySelector("#scanState");
  const list = document.querySelector("#wifiNetworks");
  button.disabled = true;
  state.textContent = "Сканування…";
  list.innerHTML = "";
  try {
    const result = await api("/api/v1/network/scan");
    const networks = Array.isArray(result.networks) ? result.networks : [];
    state.textContent = networks.length ? `Знайдено мереж: ${networks.length}` : "Мереж не знайдено";
    list.innerHTML = networks.map(item => {
      const ssid = String(item.ssid || "");
      return `<button type="button" class="wifi-network" data-ssid="${escapeHtml(ssid)}" style="display:flex;justify-content:space-between;align-items:center;width:100%;padding:11px 14px;text-align:left"><strong>${escapeHtml(ssid || "(прихована мережа)")}</strong><span>${Number(item.rssi) || 0} dBm</span></button>`;
    }).join("");
    list.querySelectorAll("[data-ssid]").forEach(item => { item.onclick = () => { document.querySelector("#wifiSsid").value = item.dataset.ssid; document.querySelector("#wifiPassword").focus(); }; });
  } catch (error) {
    state.textContent = `Помилка: ${error.message}`;
  } finally { button.disabled = false; }
}

function ensureNetworkAuthPanel() {
  if (!networkPage || document.querySelector("#networkAuth")) return;
  const connectButton = document.querySelector("#wifiConnect");
  const connectGrid = connectButton?.parentElement;
  if (!connectGrid) return;
  const panel = document.createElement("div");
  panel.id = "networkAuth";
  panel.style.cssText = "display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:10px;margin:0 0 14px";
  panel.innerHTML = `
    <label>Admin ID<input id="networkActor" type="text" maxlength="23" autocomplete="username" placeholder="ID адміністратора" style="display:block;width:100%;margin-top:6px;padding:11px;border:1px solid #d7deea;border-radius:8px"></label>
    <label>Admin PIN<input id="networkCredential" type="password" inputmode="numeric" minlength="4" maxlength="12" autocomplete="current-password" placeholder="PIN адміністратора" style="display:block;width:100%;margin-top:6px;padding:11px;border:1px solid #d7deea;border-radius:8px"></label>`;
  connectGrid.parentElement.insertBefore(panel, connectGrid);
}

async function connectWifi() {
  const ssid = document.querySelector("#wifiSsid").value.trim();
  const password = document.querySelector("#wifiPassword").value;
  const actor = document.querySelector("#networkActor")?.value.trim() || "";
  const credential = document.querySelector("#networkCredential")?.value.trim() || "";
  const button = document.querySelector("#wifiConnect");
  const result = document.querySelector("#wifiResult");
  if (!actor || !validPin(credential)) { result.textContent = "Помилка: для зміни Wi-Fi потрібні Admin ID та PIN 4–12 цифр"; return; }
  if (!ssid) { result.textContent = "Помилка: виберіть мережу"; return; }
  if (password.length > 0 && password.length < 8) { result.textContent = "Помилка: пароль має містити щонайменше 8 символів"; return; }
  button.disabled = true;
  result.textContent = "Підключення…";
  document.querySelector("#networkState").textContent = "Підключення…";
  try {
    await api("/api/v1/network/connect", { method: "POST", body: JSON.stringify({ ssid, password, actor, credential }) });
    document.querySelector("#wifiPassword").value = "";
    let connected = false;
    for (let attempt = 0; attempt < 12; attempt += 1) {
      await new Promise(resolve => setTimeout(resolve, 1500));
      const status = await refreshNetwork();
      if (status?.state === "connected") { result.textContent = `Підключено · IP ${status.ip || "—"}`; connected = true; break; }
    }
    if (!connected) {
      result.textContent = "Помилка: не вдалося підключитися. Перевірте пароль і спробуйте ще раз.";
      document.querySelector("#networkState").textContent = "Помилка";
    }
  } catch (error) {
    result.textContent = `Помилка: ${error.message}`;
    document.querySelector("#networkState").textContent = "Помилка";
  } finally {
    const pinField = document.querySelector("#networkCredential");
    if (pinField) pinField.value = "";
    button.disabled = false;
  }
}

function operatorCredentials() {
  return { actor: document.querySelector("#operatorId").value.trim(), credential: document.querySelector("#operatorPin").value.trim() };
}
function validOperator(actor, credential) {
  if (actor && validPin(credential)) return true;
  showToast("Введіть ID користувача та PIN 4–12 цифр");
  return false;
}

async function sendOutputCommand(button) {
  const outputId = Number(button.dataset.outputId);
  const active = button.dataset.outputActive === "true";
  const { actor, credential } = operatorCredentials();
  if (!Number.isInteger(outputId) || outputId <= 0 || !validOperator(actor, credential)) return;
  document.querySelectorAll("[data-output-id]").forEach(item => { item.disabled = true; });
  try {
    await api("/api/v1/system/output-command", { method: "POST", body: JSON.stringify({ outputId, active, actor, credential }) });
    showToast(active ? "Клапан відкрито" : "Клапан закрито");
  } catch (error) {
    showToast(`Помилка клапана: ${error.message}`);
  } finally {
    document.querySelector("#operatorPin").value = "";
    await refresh();
  }
}

async function sendSecurityCommand(button) {
  const command = button.dataset.command;
  const { actor, credential } = operatorCredentials();
  if (!command || !validOperator(actor, credential)) return;
  const buttons = [...document.querySelectorAll("[data-command]")];
  buttons.forEach(item => { item.disabled = true; });
  try {
    await api("/api/v1/system/security-command", { method: "POST", body: JSON.stringify({ command, actor, credential }) });
    showToast("Команду виконано");
    await refresh();
  } catch (error) {
    showToast(`Помилка команди: ${error.message}`);
  } finally {
    document.querySelector("#operatorPin").value = "";
    buttons.forEach(item => { item.disabled = false; });
  }
}

function ensureAccessPanel() {
  if (!systemPage || document.querySelector("#accessPanel")) return;
  const panel = document.createElement("article");
  panel.id = "accessPanel";
  panel.className = "panel";
  panel.style.cssText = "max-width:920px;margin-top:18px";
  panel.innerHTML = `
    <h3>Користувачі та права доступу</h3>
    <p style="margin:4px 0 14px">До 8 користувачів. Admin — повний доступ; User — моніторинг, охорона та клапани; Guest — лише перегляд стану.</p>
    <div style="display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr) auto;gap:10px;align-items:end;margin-bottom:14px">
      <label>Admin ID<input id="accessActor" type="text" maxlength="23" autocomplete="username" placeholder="ID адміністратора" style="display:block;width:100%;margin-top:6px;padding:10px;border:1px solid #d7deea;border-radius:8px"></label>
      <label>Admin PIN<input id="accessCredential" type="password" inputmode="numeric" minlength="4" maxlength="12" autocomplete="current-password" placeholder="PIN адміністратора" style="display:block;width:100%;margin-top:6px;padding:10px;border:1px solid #d7deea;border-radius:8px"></label>
      <button id="accessLoad" type="button">Оновити список</button>
    </div>
    <div id="accessUsers" style="display:grid;gap:8px;margin-bottom:16px"><small>Список ще не завантажений</small></div>
    <div style="display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px">
      <label>ID<input id="managedUserId" type="text" maxlength="23" placeholder="admin або user1" style="display:block;width:100%;margin-top:6px;padding:10px;border:1px solid #d7deea;border-radius:8px"></label>
      <label>Ім'я<input id="managedUserName" type="text" maxlength="31" placeholder="Користувач" style="display:block;width:100%;margin-top:6px;padding:10px;border:1px solid #d7deea;border-radius:8px"></label>
      <label>Роль<select id="managedUserRole" style="display:block;width:100%;margin-top:6px;padding:10px;border:1px solid #d7deea;border-radius:8px"><option value="guest">Guest</option><option value="user">User</option><option value="admin">Admin</option></select></label>
      <label>Новий PIN<input id="managedUserPin" type="password" inputmode="numeric" minlength="4" maxlength="12" placeholder="4–12 цифр" style="display:block;width:100%;margin-top:6px;padding:10px;border:1px solid #d7deea;border-radius:8px"></label>
    </div>
    <div style="display:flex;gap:12px;align-items:center;margin-top:12px;flex-wrap:wrap">
      <label><input id="managedUserEnabled" type="checkbox" checked> Активний</label>
      <button id="accessSave" type="button">Зберегти користувача</button>
      <button id="accessBootstrap" type="button">Створити першого Admin</button>
      <span id="accessResult">—</span>
    </div>
    <small style="display:block;margin-top:10px">«Створити першого Admin» працює тільки один раз — на чистому контролері без користувачів.</small>`;
  systemPage.appendChild(panel);
  document.querySelector("#accessLoad").onclick = loadAccessUsers;
  document.querySelector("#accessSave").onclick = saveAccessUser;
  document.querySelector("#accessBootstrap").onclick = bootstrapAccessAdmin;
}

function accessAdminCredentials() {
  return { actor: document.querySelector("#accessActor")?.value.trim() || "", credential: document.querySelector("#accessCredential")?.value.trim() || "" };
}
function validAdminCredentials(actor, credential) {
  if (actor && validPin(credential)) return true;
  document.querySelector("#accessResult").textContent = "Введіть Admin ID та PIN 4–12 цифр";
  return false;
}

function renderAccessUsers(data) {
  const target = document.querySelector("#accessUsers");
  const users = Array.isArray(data?.users) ? data.users : [];
  target.innerHTML = users.length ? users.map((user, index) => `
    <button type="button" data-access-user="${index}" style="display:grid;grid-template-columns:1fr 1.5fr .8fr .7fr;gap:8px;align-items:center;width:100%;padding:10px 12px;text-align:left">
      <strong>${escapeHtml(user.id)}</strong><span>${escapeHtml(user.name)}</span><span>${escapeHtml(user.role)}</span><span>${user.enabled ? "активний" : "вимкнено"}</span>
    </button>`).join("") : "<small>Користувачів немає</small>";
  target.querySelectorAll("[data-access-user]").forEach(button => {
    button.onclick = () => {
      const user = users[Number(button.dataset.accessUser)];
      if (!user) return;
      document.querySelector("#managedUserId").value = user.id || "";
      document.querySelector("#managedUserName").value = user.name || "";
      document.querySelector("#managedUserRole").value = user.role || "guest";
      document.querySelector("#managedUserEnabled").checked = user.enabled !== false;
      document.querySelector("#managedUserPin").focus();
    };
  });
}

async function bootstrapAccessAdmin() {
  const id = document.querySelector("#managedUserId").value.trim();
  const name = document.querySelector("#managedUserName").value.trim();
  const pin = document.querySelector("#managedUserPin").value.trim();
  const result = document.querySelector("#accessResult");
  if (!id || !name || !validPin(pin)) { result.textContent = "Для першого Admin введіть ID, ім'я та PIN 4–12 цифр"; return; }
  const button = document.querySelector("#accessBootstrap");
  button.disabled = true;
  result.textContent = "Створення першого Admin…";
  try {
    await api("/api/v1/access/users", { method: "POST", body: JSON.stringify({ action: "bootstrap", id, name, pin }) });
    document.querySelector("#accessActor").value = id;
    document.querySelector("#managedUserRole").value = "admin";
    result.textContent = "Першого Admin створено. Введіть його PIN вище для керування користувачами.";
  } catch (error) {
    result.textContent = `Bootstrap відхилено: ${error.message}`;
  } finally {
    document.querySelector("#managedUserPin").value = "";
    button.disabled = false;
  }
}

async function loadAccessUsers() {
  const { actor, credential } = accessAdminCredentials();
  if (!validAdminCredentials(actor, credential)) return;
  const button = document.querySelector("#accessLoad");
  const result = document.querySelector("#accessResult");
  button.disabled = true;
  result.textContent = "Завантаження…";
  try {
    const data = await api("/api/v1/access/users", { method: "POST", body: JSON.stringify({ actor, credential, action: "list" }) });
    renderAccessUsers(data);
    result.textContent = `Користувачів: ${Number(data.count) || 0} / ${Number(data.capacity) || 8}`;
  } catch (error) {
    result.textContent = `Помилка: ${error.message}`;
  } finally {
    document.querySelector("#accessCredential").value = "";
    button.disabled = false;
  }
}

async function saveAccessUser() {
  const { actor, credential } = accessAdminCredentials();
  const id = document.querySelector("#managedUserId").value.trim();
  const name = document.querySelector("#managedUserName").value.trim();
  const role = document.querySelector("#managedUserRole").value;
  const pin = document.querySelector("#managedUserPin").value.trim();
  const enabled = document.querySelector("#managedUserEnabled").checked;
  const result = document.querySelector("#accessResult");
  if (!validAdminCredentials(actor, credential)) return;
  if (!id || !name || !["admin", "user", "guest"].includes(role) || !validPin(pin)) { result.textContent = "Перевірте ID, ім'я, роль і PIN користувача"; return; }
  const button = document.querySelector("#accessSave");
  button.disabled = true;
  result.textContent = "Збереження…";
  try {
    await api("/api/v1/access/users", { method: "POST", body: JSON.stringify({ actor, credential, action: "set", id, name, role, pin, enabled }) });
    result.textContent = "Користувача збережено";
    document.querySelector("#managedUserPin").value = "";
  } catch (error) {
    result.textContent = `Помилка: ${error.message}`;
  } finally {
    document.querySelector("#accessCredential").value = "";
    button.disabled = false;
  }
}

function setView(view, targetId = "overview") {
  const isNetwork = view === "network";
  const isSystem = view === "system";
  dashboardSections.forEach(section => { section.hidden = isNetwork || isSystem; });
  networkPage.hidden = !isNetwork;
  systemPage.hidden = !isSystem;
  if (!isNetwork && !isSystem && targetId && targetId !== "overview") {
    requestAnimationFrame(() => document.getElementById(targetId)?.scrollIntoView({ behavior: "smooth", block: "start" }));
  } else { window.scrollTo({ top: 0, behavior: "smooth" }); }
  if (isNetwork) refreshNetwork();
  if (isSystem) { ensureAccessPanel(); refresh(); }
}

function routeFromHash() {
  const hash = window.location.hash || "#overview";
  if (hash === "#networkPage") setView("network");
  else if (hash === "#system") setView("system");
  else setView("dashboard", hash.slice(1));
  document.querySelectorAll(".sidebar nav a").forEach(link => link.classList.toggle("active", link.getAttribute("href") === hash || (hash === "#overview" && link.getAttribute("href") === "#overview")));
}

function bindNavigation() {
  document.querySelectorAll(".sidebar nav a").forEach(link => {
    link.addEventListener("click", event => {
      event.preventDefault();
      const href = link.getAttribute("href") || "#overview";
      if (window.location.hash === href) routeFromHash(); else window.location.hash = href;
    });
  });
  document.querySelector("#networkCard").addEventListener("click", () => { window.location.hash = "#networkPage"; });
  window.addEventListener("hashchange", routeFromHash);
}

function bindCommandButtons() {
  document.querySelectorAll("[data-command]").forEach(button => { button.onclick = () => sendSecurityCommand(button); });
}

function bootUi() {
  ensureNetworkAuthPanel();
  ensureAccessPanel();
  document.querySelector("#wifiScan").onclick = scanWifi;
  document.querySelector("#wifiConnect").onclick = connectWifi;
  document.querySelector("#refresh").onclick = refresh;
  bindNavigation();
  bindCommandButtons();
  routeFromHash();
  refresh();
  setInterval(refresh, 5000);
  document.documentElement.dataset.homeguardUi = "ready";
}

bootUi();
