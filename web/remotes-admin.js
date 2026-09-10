"use strict";

(() => {
  const API = "/api/v1/access/remotes";
  let cache = { count: 0, capacity: 8, users: [], remotes: [] };

  function esc(value) {
    return String(value ?? "").replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/\"/g, "&quot;").replace(/'/g, "&#39;");
  }

  function actor() {
    return window.HomeGuardAuth?.actor?.() || "";
  }

  function isAdmin() {
    return window.HomeGuardAuth?.role?.() === "admin";
  }

  async function request(options = {}) {
    const response = await fetch(API, {
      cache: "no-store",
      ...options,
      headers: { "Content-Type": "application/json", ...(options.headers || {}) },
    });
    const body = await response.json().catch(() => ({}));
    if (!response.ok || body.ok === false) throw new Error(body.reason || `${response.status}`);
    return body;
  }

  function permissionSummary(p = {}) {
    const labels = [];
    if (p.armHome) labels.push("Нічний");
    if (p.armAway) labels.push("Охорона");
    if (p.disarm) labels.push("Зняти");
    if (p.light) labels.push("Світло");
    if (p.lockPulse) labels.push("Замок");
    if (p.panic) labels.push("SOS");
    return labels.length ? labels.join(" · ") : "Без команд";
  }

  function ensurePanel() {
    if (!isAdmin()) return null;
    let panel = document.querySelector("#bleRemotesAdmin");
    if (panel) return panel;
    const system = document.querySelector("#system");
    if (!system) return null;
    panel = document.createElement("div");
    panel.id = "bleRemotesAdmin";
    panel.className = "panel";
    panel.style.marginTop = "18px";
    panel.innerHTML = `
      <div style="display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap">
        <div><h3 style="margin:0">BLE брелки</h3><small id="bleRemoteCount">0 / 8</small></div>
        <button id="bleRemoteAdd" type="button">＋ Додати брелок</button>
      </div>
      <div id="bleRemoteState" style="margin:10px 0;color:#667085">Завантаження…</div>
      <div id="bleRemoteList"></div>
      <div id="bleRemoteEditor" hidden style="margin-top:14px;padding-top:14px;border-top:1px solid #e4e7ec">
        <h4 id="bleRemoteEditorTitle">Додати брелок</h4>
        <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:10px">
          <label>Назва<input id="bleRemoteName" maxlength="31" placeholder="Напр. Брелок Віктора" style="display:block;width:100%;margin-top:5px"></label>
          <label>Користувач<select id="bleRemoteOwner" style="display:block;width:100%;margin-top:5px"></select></label>
          <label style="grid-column:1/-1">BLE ID<input id="bleRemoteIdentity" maxlength="32" placeholder="32 hex символи" style="display:block;width:100%;margin-top:5px;font-family:monospace"></label>
        </div>
        <div style="display:flex;gap:12px;flex-wrap:wrap;margin:12px 0" id="bleRemotePermissions">
          <label><input type="checkbox" data-perm="armHome"> Нічний режим</label>
          <label><input type="checkbox" data-perm="armAway"> Під охорону</label>
          <label><input type="checkbox" data-perm="disarm"> Зняти</label>
          <label><input type="checkbox" data-perm="light"> Світло</label>
          <label><input type="checkbox" data-perm="lockPulse"> Замок 5 с</label>
          <label><input type="checkbox" data-perm="panic"> SOS</label>
        </div>
        <div style="display:flex;gap:8px"><button id="bleRemoteSave" type="button">Зберегти</button><button id="bleRemoteCancel" type="button">Скасувати</button></div>
      </div>`;
    system.appendChild(panel);
    panel.querySelector("#bleRemoteAdd").onclick = () => editRemote(null);
    panel.querySelector("#bleRemoteCancel").onclick = closeEditor;
    panel.querySelector("#bleRemoteSave").onclick = saveRemote;
    return panel;
  }

  function render() {
    const panel = ensurePanel();
    if (!panel) return;
    panel.querySelector("#bleRemoteCount").textContent = `${cache.count || 0} / ${cache.capacity || 8}`;
    const list = panel.querySelector("#bleRemoteList");
    const state = panel.querySelector("#bleRemoteState");
    const remotes = Array.isArray(cache.remotes) ? cache.remotes : [];
    state.textContent = remotes.length ? `У системі брелків: ${remotes.length}` : "Брелків ще немає";
    list.innerHTML = remotes.map((remote, index) => `
      <div style="display:grid;grid-template-columns:minmax(140px,1.2fr) minmax(140px,1fr) minmax(180px,2fr) auto;gap:10px;align-items:center;padding:11px 0;border-top:1px solid #eef0f3">
        <div><strong>${esc(remote.name)}</strong><small style="display:block;font-family:monospace">${esc(remote.identity)}</small></div>
        <div><small>Користувач</small><strong style="display:block">${esc(remote.ownerName || remote.ownerUserId)}</strong>${remote.ownerEnabled === false ? '<small style="color:#b42318">Вимкнений</small>' : ""}</div>
        <div><small>Дозволено</small><span style="display:block">${esc(permissionSummary(remote.permissions))}</span></div>
        <div style="display:flex;gap:6px"><button type="button" data-edit="${index}">Змінити</button><button type="button" data-delete="${index}">Видалити</button></div>
      </div>`).join("");
    list.querySelectorAll("[data-edit]").forEach(button => button.onclick = () => editRemote(remotes[Number(button.dataset.edit)]));
    list.querySelectorAll("[data-delete]").forEach(button => button.onclick = () => removeRemote(remotes[Number(button.dataset.delete)]));
  }

  function fillUsers(selected = "") {
    const select = document.querySelector("#bleRemoteOwner");
    if (!select) return;
    const users = (cache.users || []).filter(user => user.enabled !== false);
    select.innerHTML = users.map(user => `<option value="${esc(user.id)}" ${user.id === selected ? "selected" : ""}>${esc(user.name || user.id)} (${esc(user.role)})</option>`).join("");
  }

  function editRemote(remote) {
    const panel = ensurePanel();
    if (!panel) return;
    const editor = panel.querySelector("#bleRemoteEditor");
    editor.hidden = false;
    panel.querySelector("#bleRemoteEditorTitle").textContent = remote ? "Змінити брелок" : "Додати брелок";
    panel.querySelector("#bleRemoteIdentity").value = remote?.identity || "";
    panel.querySelector("#bleRemoteIdentity").readOnly = Boolean(remote);
    panel.querySelector("#bleRemoteName").value = remote?.name || "";
    fillUsers(remote?.ownerUserId || "");
    panel.querySelectorAll("[data-perm]").forEach(box => { box.checked = Boolean(remote?.permissions?.[box.dataset.perm]); });
    panel.querySelector("#bleRemoteName").focus();
  }

  function closeEditor() {
    const editor = document.querySelector("#bleRemoteEditor");
    if (editor) editor.hidden = true;
  }

  async function saveRemote() {
    const panel = ensurePanel();
    if (!panel) return;
    const name = panel.querySelector("#bleRemoteName").value.trim();
    const ownerUserId = panel.querySelector("#bleRemoteOwner").value;
    const identity = panel.querySelector("#bleRemoteIdentity").value.trim().toLowerCase();
    if (!name || !/^[0-9a-f]{32}$/.test(identity) || !ownerUserId) {
      panel.querySelector("#bleRemoteState").textContent = "Заповни назву, користувача і правильний BLE ID (32 hex символи)";
      return;
    }
    const payload = { actor: actor(), identity, ownerUserId, name };
    panel.querySelectorAll("[data-perm]").forEach(box => { payload[box.dataset.perm] = box.checked; });
    try {
      await request({ method: "POST", body: JSON.stringify(payload) });
      closeEditor();
      await refresh();
    } catch (error) {
      panel.querySelector("#bleRemoteState").textContent = `Не збережено: ${error.message}`;
    }
  }

  async function removeRemote(remote) {
    if (!remote || !confirm(`Видалити брелок «${remote.name}»?`)) return;
    try {
      await request({ method: "DELETE", body: JSON.stringify({ actor: actor(), identity: remote.identity }) });
      await refresh();
    } catch (error) {
      const state = document.querySelector("#bleRemoteState");
      if (state) state.textContent = `Не видалено: ${error.message}`;
    }
  }

  async function refresh() {
    if (!isAdmin()) return;
    const panel = ensurePanel();
    if (!panel) return;
    try {
      cache = await request();
      render();
    } catch (error) {
      panel.querySelector("#bleRemoteState").textContent = `Брелки недоступні: ${error.message}`;
    }
  }

  function refreshWhenRelevant() {
    if (window.location.hash === "#system" && isAdmin()) void refresh();
  }

  window.HomeGuardRemotes = { refresh };
  window.addEventListener("hashchange", refreshWhenRelevant);
  document.addEventListener("visibilitychange", () => { if (!document.hidden) refreshWhenRelevant(); });
  document.addEventListener("click", event => {
    if (event.target.closest?.('a[href="#system"]')) setTimeout(refreshWhenRelevant, 0);
  });
})();

/* Canonical dashboard live-runtime repair.
   Keep this after app.js/access-session.js so it is the final authority for
   live zones, relay buttons and transport truth indicators. */
(() => {
  const q = id => document.getElementById(id);
  const authenticated = () => window.HomeGuardAuth?.authenticated?.() === true;
  const actor = () => window.HomeGuardAuth?.actor?.() || "";
  const esc = value => String(value ?? "").replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/\"/g,"&quot;").replace(/'/g,"&#39;");

  async function json(path, options = {}) {
    const response = await fetch(path, {
      cache: "no-store",
      ...options,
      headers: { "Content-Type": "application/json", ...(options.headers || {}) }
    });
    const text = await response.text();
    let body = {};
    try { body = text ? JSON.parse(text) : {}; } catch (_) {}
    if (!response.ok || body.ok === false) throw new Error(body.reason || `${response.status}`);
    return body;
  }

  function health(id, connected, known = true) {
    const node = q(id);
    if (!node) return;
    node.textContent = !known ? "—" : (connected ? "OK" : "OFF");
    node.className = `hg-health ${!known ? "wait" : (connected ? "ok" : "bad")}`;
  }

  function zoneKind(zone) {
    if (zone?.valid === false) return "alarm";
    if (zone?.state === "normal") return "ok";
    if (zone?.state === "short") return "warning";
    return "alarm";
  }

  function zoneLabel(zone) {
    if (zone?.valid === false) return "Обрив";
    return ({ normal: "Норма", short: "КЗ", open: "Обрив" })[zone?.state] || "Обрив";
  }

  async function refreshLiveZones() {
    if (!authenticated()) return;
    try {
      const data = await json("/api/v1/zones/live");
      const zones = Array.isArray(data?.zones) ? data.zones.slice(0, 8) : [];
      const target = q("zones");
      if (!target) return;
      if (q("zoneCount")) q("zoneCount").textContent = String(zones.length || "—");
      target.innerHTML = zones.map(zone => {
        const kind = zoneKind(zone);
        const mv = Number(zone.mv);
        const title = Number.isFinite(mv) ? `${mv.toFixed(1)} mV` : "немає даних";
        return `<div class="zone" title="${esc(title)}"><i class="hg-zone-dot ${kind}"></i><b class="hg-zone-id">${Number(zone.id) || "—"}</b><span class="hg-zone-name">${esc(zone.name || `Зона ${zone.id}`)}</span><strong class="hg-zone-state ${kind}">${zoneLabel(zone)}</strong><span class="hg-zone-arrow">›</span></div>`;
      }).join("");
    } catch (_) {
      const target = q("zones");
      if (target) target.querySelectorAll(".hg-zone-state").forEach(node => { node.textContent = "—"; node.className = "hg-zone-state warning"; });
    }
  }

  function applyRelayState(state) {
    const light = Boolean(state?.lightActive);
    const lock = Boolean(state?.lockActive);
    const lightButton = q("hgQuickLight");
    if (lightButton) lightButton.dataset.outputActive = String(!light);
    if (q("hgQuickLightState")) q("hgQuickLightState").textContent = light ? (state?.lightAutomatic ? "ON · AUTO" : "ON") : "OFF";
    if (q("hgQuickLockState")) {
      const remaining = Number(state?.lockRemainingMs || 0);
      q("hgQuickLockState").textContent = lock ? `ON · ${(remaining / 1000).toFixed(1)} с` : "OFF";
    }
  }

  async function refreshRelays() {
    if (!authenticated()) return;
    try { applyRelayState(await json("/api/v1/outputs/relay-state")); } catch (_) {}
  }

  async function relayCommand(kind, button) {
    if (!authenticated() || !actor()) return;
    if (button) button.disabled = true;
    try {
      if (kind === "light") {
        const active = button?.dataset?.outputActive === "true";
        const state = await json("/api/v1/outputs/light", { method: "POST", body: JSON.stringify({ actor: actor(), active }) });
        applyRelayState(state);
        if (typeof showToast === "function") showToast(active ? "Освітлення увімкнено" : "Освітлення вимкнено");
      } else {
        const state = await json("/api/v1/outputs/lock/pulse", { method: "POST", body: JSON.stringify({ actor: actor() }) });
        applyRelayState(state);
        if (typeof showToast === "function") showToast("Замок відкрито на 5 секунд");
      }
    } catch (error) {
      if (typeof showToast === "function") showToast(`Помилка реле: ${error.message}`);
    } finally {
      if (button) button.disabled = false;
      setTimeout(refreshRelays, 120);
    }
  }

  // Capture before the legacy target handlers, which still point at the old
  // generic output-command path.
  document.addEventListener("click", event => {
    const light = event.target.closest?.("#hgQuickLight");
    const lock = event.target.closest?.("#hgQuickLock");
    if (!light && !lock) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    void relayCommand(light ? "light" : "lock", light || lock);
  }, true);

  function setText(id, value) { const node = q(id); if (node) node.textContent = value; }

  async function refreshTruthStatus() {
    if (!authenticated()) return;
    const results = await Promise.allSettled([
      json("/api/v1/network/status"),
      json("/api/v1/hardware/status"),
      json("/api/v1/cloud/status")
    ]);

    if (results[0].status === "fulfilled") {
      const wifi = results[0].value || {};
      const connected = wifi.state === "connected" && Boolean(wifi.ip && wifi.ip !== "—");
      setText("wifiName", wifi.ssid || "—");
      setText("connection", `IP ${wifi.ip || "—"}`);
      setText("hgInfoWifi", `${wifi.ssid || "—"}${wifi.ip ? ` · ${wifi.ip}` : ""}`);
      health("hgWifiHealth", connected, true);
    } else {
      health("hgWifiHealth", false, false);
    }

    if (results[1].status === "fulfilled") {
      const d = results[1].value?.dashboard || {};
      const lan = d.lan || {};
      const ble = d.ble || {};
      const lanConnected = lan.state === "connected" && Boolean(lan.ip);
      const btConnected = ble.connected === true;
      setText("hgLanName", lan.name || "Ethernet");
      setText("hgLanIp", `IP ${lan.ip || "—"}`);
      setText("hgBtName", ble.name || "HomeGuard-S3");
      setText("hgBtDetail", ble.address ? `MAC ${ble.address}` : "MAC —");
      setText("hgInfoLan", `${lan.name || "Ethernet"}${lan.ip ? ` · ${lan.ip}` : ""}`);
      setText("hgInfoBt", `${ble.name || "HomeGuard-S3"}${ble.address ? ` · ${ble.address}` : ""}`);
      health("hgLanHealth", lanConnected, true);
      health("hgBtHealth", btConnected, true);
    } else {
      health("hgLanHealth", false, false);
      health("hgBtHealth", false, false);
    }

    if (results[2].status === "fulfilled") {
      const cloud = results[2].value || {};
      const broker = String(cloud.brokerUri || "").replace(/^mqtts?:\/\//, "").split("/")[0] || "MQTT";
      const connected = cloud.connected === true;
      setText("cloudState", broker);
      setText("cloudDetail", connected ? "Підключено" : (cloud.configured ? "Не підключено" : "Не налаштовано"));
      setText("hgInfoMqtt", broker);
      health("hgMqttHealth", connected, true);
    } else {
      health("hgMqttHealth", false, false);
    }
  }

  const compact = document.createElement("style");
  compact.id = "hgCanonicalCompactViewport";
  compact.textContent = `
    @media (min-width:1101px){
      .workspace header{height:72px!important;padding:10px 24px!important}.workspace header h2{font-size:27px!important}.workspace header p{margin-top:1px!important;font-size:13px!important}
      main{padding:14px 22px 18px!important}.status-grid.hg-transport-strip article{min-height:78px!important;padding:9px 15px!important;gap:11px!important}.hg-transport-icon{width:44px!important;height:44px!important;flex-basis:44px!important;font-size:31px!important}.hg-transport-copy>strong{font-size:16px!important}.hg-transport-copy>small{font-size:12px!important}
      .two-col.hg-dashboard-grid{gap:10px!important;margin-top:10px!important;grid-template-columns:minmax(0,1.55fr) minmax(330px,.8fr)!important}.hg-dashboard-grid .hg-quick-panel{padding:10px 12px!important}.hg-dashboard-grid .hg-quick-panel h3{margin-bottom:7px!important}.hg-dashboard-grid .quick{gap:7px!important}.hg-dashboard-grid .quick button{height:70px!important;grid-template-columns:48px 1fr!important;padding:7px 12px!important;column-gap:8px!important}.hg-dashboard-grid .quick button b{font-size:31px!important}.hg-dashboard-grid .quick button strong{font-size:15px!important}.hg-dashboard-grid .quick button small{font-size:12px!important}
      .hg-quick-secondary{gap:7px!important;margin-top:7px!important}.hg-quick-secondary button,.hg-quick-secondary .hg-quick-state{height:58px!important;gap:1px!important}.hg-quick-secondary b{font-size:21px!important}.hg-quick-secondary strong{font-size:14px!important}.hg-quick-secondary small{font-size:11px!important}
      .hg-dashboard-grid #zones-section,.hg-dashboard-grid #io-section,.hg-dashboard-grid #events,.hg-dashboard-grid #hgDeviceInfo{padding:10px 12px!important}.hg-dashboard-grid .panel h3{font-size:17px!important;margin-bottom:7px!important}
      #zones.zones{grid-template-rows:repeat(4,38px)!important;gap:3px 10px!important}#zones .zone{height:38px!important;padding:0 8px!important;grid-template-columns:10px 22px minmax(0,1fr) auto 10px!important;gap:6px!important}.hg-zone-state{font-size:12px!important}.hg-zone-arrow{font-size:16px!important}
      #ioState.io{gap:6px!important}#ioState.io>div{min-height:58px!important;padding:5px!important;gap:1px!important}#ioState.io b{font-size:20px!important}#ioState.io small{font-size:11px!important}
      .hg-device-row{min-height:25px!important;font-size:12px!important;grid-template-columns:102px minmax(0,1fr) 40px!important}.events>div{min-height:30px!important;padding:4px 0!important;font-size:12px!important}.history{padding-top:6px!important;margin-top:4px!important;font-size:12px!important}
    }`;
  document.head.appendChild(compact);

  let timer = 0;
  async function tick() {
    if (authenticated() && !document.hidden) {
      await Promise.allSettled([refreshLiveZones(), refreshRelays(), refreshTruthStatus()]);
    }
    timer = window.setTimeout(tick, 750);
  }
  window.addEventListener("focus", () => { if (authenticated()) { void refreshLiveZones(); void refreshRelays(); void refreshTruthStatus(); } });
  document.addEventListener("visibilitychange", () => { if (!document.hidden && authenticated()) { void refreshLiveZones(); void refreshRelays(); void refreshTruthStatus(); } });
  window.setTimeout(tick, 600);
})();
