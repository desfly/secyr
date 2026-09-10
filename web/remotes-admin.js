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

/* Dashboard runtime: keep the approved visual shell, but restore the exact
   known-good direct relay path and a single fast live-zone reader. */
(() => {
  const ZONE_POLL_MS = 100;
  const RELAY_POLL_MS = 200;
  let liveRelayState = null;
  let zoneBusy = false;
  let relayBusy = false;
  let lastZoneSignature = "";

  const q = id => document.getElementById(id);
  const isAuthenticated = () => window.HomeGuardAuth?.authenticated?.() === true;
  const isAdmin = () => window.HomeGuardAuth?.role?.() === "admin";
  const actor = () => window.HomeGuardAuth?.actor?.() || "";
  const esc = value => String(value ?? "").replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/\"/g,"&quot;").replace(/'/g,"&#39;");

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

  function renderLiveZones(zones) {
    const target = q("zones");
    if (!target) return;
    const list = Array.isArray(zones) ? zones.slice(0, 8) : [];
    const signature = list.map(zone => `${zone.id}:${zone.name}:${zone.valid}:${zone.state}`).join("|");
    if (signature === lastZoneSignature) return;
    lastZoneSignature = signature;
    const count = q("zoneCount");
    if (count) count.textContent = String(list.length || "—");
    target.innerHTML = list.map(zone => {
      const kind = zoneKind(zone);
      const mv = Number(zone.mv);
      const title = Number.isFinite(mv) ? `${mv.toFixed(1)} mV` : "немає даних";
      return `<div class="zone" title="${esc(title)}"><i class="hg-zone-dot ${kind}"></i><b class="hg-zone-id">${Number(zone.id) || "—"}</b><span class="hg-zone-name">${esc(zone.name || `Зона ${zone.id}`)}</span><strong class="hg-zone-state ${kind}">${zoneLabel(zone)}</strong><span class="hg-zone-arrow">›</span></div>`;
    }).join("");
  }

  async function refreshLiveZones() {
    if (zoneBusy || !isAuthenticated() || document.hidden) return;
    zoneBusy = true;
    try {
      const data = await api("/api/v1/zones/live");
      renderLiveZones(data?.zones);
    } catch (_) {
    } finally {
      zoneBusy = false;
    }
  }

  function renderRelayState(state) {
    liveRelayState = state && state.ok !== false ? state : null;
    const light = q("hgQuickLight");
    const lock = q("hgQuickLock");
    const admin = isAdmin();
    if (light) light.disabled = !admin || !liveRelayState;
    if (lock) lock.disabled = !admin || !liveRelayState;
    if (!liveRelayState) {
      if (q("hgQuickLightState")) q("hgQuickLightState").textContent = "—";
      if (q("hgQuickLockState")) q("hgQuickLockState").textContent = "—";
      return;
    }

    const lightOn = liveRelayState.lightActive === true;
    const manual = liveRelayState.lightManual === true;
    const automatic = liveRelayState.lightAutomatic === true;
    if (q("hgQuickLightState")) {
      q("hgQuickLightState").textContent = lightOn
        ? (automatic && !manual ? "AUTO ON · Z1/Z2" : manual ? (automatic ? "MANUAL + AUTO ON" : "MANUAL ON") : "ON")
        : "OFF";
    }
    if (light) {
      light.setAttribute("aria-pressed", String(lightOn));
      const icon = light.querySelector("b");
      if (icon) icon.textContent = lightOn ? "●" : "○";
      light.style.boxShadow = lightOn ? "inset 0 0 0 2px #22c55e" : "";
    }

    const lockOn = liveRelayState.lockActive === true;
    const seconds = Math.max(0, Math.ceil(Number(liveRelayState.lockRemainingMs || 0) / 1000));
    if (q("hgQuickLockState")) q("hgQuickLockState").textContent = lockOn ? `ON · ${seconds} с` : "OFF";
    if (lock) {
      lock.setAttribute("aria-pressed", String(lockOn));
      lock.style.boxShadow = lockOn ? "inset 0 0 0 2px #f59e0b" : "";
    }
  }

  async function refreshRelayState() {
    if (relayBusy || !isAuthenticated() || document.hidden) return;
    relayBusy = true;
    try {
      renderRelayState(await api("/api/v1/outputs/relay-state"));
    } catch (_) {
      renderRelayState(null);
    } finally {
      relayBusy = false;
    }
  }

  async function setManualLight(button) {
    if (!liveRelayState || !isAdmin()) return;
    button.disabled = true;
    try {
      const state = await api("/api/v1/outputs/light", {
        method: "POST",
        body: JSON.stringify({ actor: actor(), active: liveRelayState.lightManual !== true })
      });
      renderRelayState(state);
      showToast(state.lightActive ? "Освітлення ON" : "Освітлення OFF");
    } catch (error) {
      showToast(`Освітлення: ${error.message}`);
    } finally {
      button.disabled = false;
      void refreshRelayState();
    }
  }

  async function pulseLock(button) {
    if (!isAdmin()) return;
    button.disabled = true;
    try {
      renderRelayState(await api("/api/v1/outputs/lock/pulse", {
        method: "POST",
        body: JSON.stringify({ actor: actor() })
      }));
      showToast("Замок ON · 5 секунд");
    } catch (error) {
      showToast(`Замок: ${error.message}`);
    } finally {
      button.disabled = false;
      void refreshRelayState();
    }
  }

  function replaceLegacyQuickButton(id, handler) {
    const old = q(id);
    if (!old) return null;
    const button = old.cloneNode(true);
    button.removeAttribute("data-output-id");
    button.removeAttribute("data-output-active");
    button.dataset.relayDirect = "true";
    old.replaceWith(button);
    button.addEventListener("click", event => {
      event.preventDefault();
      event.stopPropagation();
      void handler(button);
    });
    return button;
  }

  function installDirectRelayBindings() {
    replaceLegacyQuickButton("hgQuickLight", setManualLight);
    replaceLegacyQuickButton("hgQuickLock", pulseLock);
    document.documentElement.dataset.homeguardRelayPath = "direct-runtime";
  }

  function installCompactViewport() {
    if (q("hgCanonicalCompactViewport")) return;
    const style = document.createElement("style");
    style.id = "hgCanonicalCompactViewport";
    style.textContent = `
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
    document.head.appendChild(style);
  }

  // The old 5-second SystemModel renderer must never overwrite live ADS1115
  // zone state. Redirect it to the same single live source used below.
  if (typeof renderZones === "function") {
    renderZones = () => { void refreshLiveZones(); };
  }

  installDirectRelayBindings();
  installCompactViewport();
  void refreshLiveZones();
  void refreshRelayState();

  async function zoneLoop() {
    await refreshLiveZones();
    window.setTimeout(zoneLoop, ZONE_POLL_MS);
  }
  async function relayLoop() {
    await refreshRelayState();
    window.setTimeout(relayLoop, RELAY_POLL_MS);
  }
  window.setTimeout(zoneLoop, ZONE_POLL_MS);
  window.setTimeout(relayLoop, RELAY_POLL_MS);
  window.addEventListener("focus", () => { void refreshLiveZones(); void refreshRelayState(); });
})();
