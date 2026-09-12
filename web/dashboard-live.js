"use strict";

(() => {
  const live = {
    zones: null,
    zonesMv: null,
    zonesAt: 0,
    zoneMeta: [],
    outputs: new Map(),
    outputTimer: 0,
    analogTimer: 0,
    authTimer: 0,
    connectivityTimer: 0,
    lockBusy: false,
    commandBusy: false,
  };

  const style = document.createElement("style");
  style.id = "hg-dashboard-live-contract";
  style.textContent = `
    .zone.hg-zone-normal:before{color:#0aaa42!important}.zone.hg-zone-normal strong{color:#0aaa42!important}
    .zone.hg-zone-open:before{color:#e31b23!important}.zone.hg-zone-open strong{color:#e31b23!important}
    .zone.hg-zone-short:before{color:#e0a000!important}.zone.hg-zone-short strong{color:#b98000!important}
    .zone.hg-zone-unavailable:before{color:#8a94a6!important}.zone.hg-zone-unavailable strong{color:#6d778c!important}
    #quickLight[data-active="true"],#quickLock[data-active="true"]{border-color:#0aaa42!important;background:#edf9f1!important}
    #quickLight[data-active="true"] small,#quickLock[data-active="true"] small{color:#078c37!important;font-weight:800}
    #quickLight[data-active="false"] small,#quickLock[data-active="false"] small{font-weight:700}
    #quickLight:disabled,#quickLock:disabled{opacity:.7;cursor:wait}
    #adminConnectivityCard>div:first-child{margin-bottom:4px!important}
    #adminConnectivityCard>div:nth-child(2)>div{padding:5px 8px!important}
    #commWifiState,#commEthState,#commBleState,#commCloudState{display:grid!important;place-items:center!important}
    #commWifiState strong,#commEthState strong,#commBleState strong,#commCloudState strong{
      display:block!important;width:13px!important;height:13px!important;min-width:13px!important;padding:0!important;border-radius:50%!important;font-size:0!important;color:transparent!important;overflow:hidden!important
    }
    #adminConnectivityCard strong[style*="#dcf8e8"]{background:#0aaa42!important}
    #adminConnectivityCard strong[style*="#ffe3e3"]{background:#e31b23!important}
    #adminConnectivityCard strong[style*="#fff4cc"]{background:#e0a000!important}
  `;
  document.head.appendChild(style);

  const escape = value => String(value ?? "")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/\"/g, "&quot;")
    .replace(/'/g, "&#39;");

  function sessionActor() {
    return window.HomeGuardAuth?.actor?.() || "";
  }

  function requireSessionActor() {
    if (!window.HomeGuardAuth?.authenticated?.()) throw new Error("потрібен активний сеанс");
    const actor = sessionActor();
    if (!actor) throw new Error("немає користувача активного сеансу");
    return actor;
  }

  function ensureSessionQuickControls() {
    const quick = document.querySelector(".quick");
    if (!quick) return;

    if (!document.getElementById("quickLight")) {
      quick.insertAdjacentHTML("beforeend", `
        <button id="quickLight" type="button" data-active="false" aria-pressed="false"><b class="orange-text">☀</b><strong>Світло</strong><small>ВИМКНЕНО</small></button>
        <button id="quickLock" type="button" data-active="false" aria-pressed="false"><b class="blue-text">⌑</b><strong>Замок</strong><small>ЗАКРИТО</small></button>`);
    }

    const operatorId = document.getElementById("operatorId");
    const operatorPin = document.getElementById("operatorPin");
    const authPanel = operatorId?.closest("div");
    if (authPanel && operatorPin && authPanel.contains(operatorPin)) {
      authPanel.hidden = true;
      authPanel.style.setProperty("display", "none", "important");
    }
  }

  function connectivityBadge(online, pending = false) {
    const text = pending ? "ПІДКЛЮЧЕННЯ" : (online ? "ОНЛАЙН" : "ОФЛАЙН");
    const background = pending ? "#fff4cc" : (online ? "#dcf8e8" : "#ffe3e3");
    const foreground = pending ? "#8a6500" : (online ? "#157347" : "#b42318");
    return `<strong style="display:inline-block;min-width:104px;padding:5px 9px;border-radius:999px;text-align:center;background:${background};color:${foreground}">${text}</strong>`;
  }

  function restoreStandaloneConnectivityCards() {
    const wifi = document.getElementById("networkCard");
    const cloud = document.getElementById("cloudCard");
    if (wifi) {
      wifi.hidden = false;
      wifi.style.removeProperty("display");
    }
    if (cloud) {
      cloud.hidden = false;
      cloud.style.removeProperty("display");
    }
  }

  function removeAdminConnectivityCard() {
    document.getElementById("adminConnectivityCard")?.remove();
    restoreStandaloneConnectivityCards();
    if (live.connectivityTimer) {
      window.clearInterval(live.connectivityTimer);
      live.connectivityTimer = 0;
    }
  }

  function ensureAdminConnectivityCard() {
    if (!window.HomeGuardAuth?.authenticated?.() || window.HomeGuardAuth?.role?.() !== "admin") {
      removeAdminConnectivityCard();
      return null;
    }

    const grid = document.querySelector(".status-grid");
    if (!grid) return null;

    const wifi = document.getElementById("networkCard");
    const cloud = document.getElementById("cloudCard");
    if (wifi) {
      wifi.hidden = true;
      wifi.style.setProperty("display", "none", "important");
    }
    if (cloud) {
      cloud.hidden = true;
      cloud.style.setProperty("display", "none", "important");
    }

    let card = document.getElementById("adminConnectivityCard");
    if (card) return card;

    card = document.createElement("article");
    card.id = "adminConnectivityCard";
    card.style.cssText = "grid-column:span 2;display:block;min-height:0;padding:16px 18px";
    card.innerHTML = `
      <div style="display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:10px">
        <div><span style="font-size:13px;color:#66758b">Admin</span><strong style="display:block;font-size:18px">Зв'язок</strong></div>
        <small id="adminConnectivityUpdated">—</small>
      </div>
      <div style="display:grid;grid-template-columns:minmax(120px,1fr) 130px minmax(120px,1fr);gap:0;border:1px solid #d7deea;border-radius:10px;overflow:hidden">
        <div style="padding:8px 10px;background:#f5f7fa;font-weight:700">Канал</div><div style="padding:8px 10px;background:#f5f7fa;font-weight:700">Стан</div><div style="padding:8px 10px;background:#f5f7fa;font-weight:700">IP / адреса</div>
        <div style="padding:10px;border-top:1px solid #e2e7ef;font-weight:700">Wi-Fi</div><div id="commWifiState" style="padding:10px;border-top:1px solid #e2e7ef">—</div><div id="commWifiIp" style="padding:10px;border-top:1px solid #e2e7ef">—</div>
        <div style="padding:10px;border-top:1px solid #e2e7ef;font-weight:700">Ethernet W5500</div><div id="commEthState" style="padding:10px;border-top:1px solid #e2e7ef">—</div><div id="commEthIp" style="padding:10px;border-top:1px solid #e2e7ef">—</div>
        <div style="padding:10px;border-top:1px solid #e2e7ef;font-weight:700">Bluetooth BLE</div><div id="commBleState" style="padding:10px;border-top:1px solid #e2e7ef">—</div><div id="commBleIp" style="padding:10px;border-top:1px solid #e2e7ef">—</div>
        <div style="padding:10px;border-top:1px solid #e2e7ef;font-weight:700">Cloud MQTT</div><div id="commCloudState" style="padding:10px;border-top:1px solid #e2e7ef">—</div><div id="commCloudIp" style="padding:10px;border-top:1px solid #e2e7ef">—</div>
      </div>`;
    grid.appendChild(card);
    return card;
  }

  async function refreshAdminConnectivity() {
    if (!window.HomeGuardAuth?.authenticated?.() || window.HomeGuardAuth?.role?.() !== "admin") {
      removeAdminConnectivityCard();
      return;
    }
    if (!ensureAdminConnectivityCard()) return;

    try {
      const [wifi, local, cloud] = await Promise.all([
        api("/api/v1/network/status"),
        api("/api/v1/connectivity/status"),
        api("/api/v1/cloud/status")
      ]);

      const wifiOnline = wifi?.state === "connected" || local?.wifi?.online === true;
      const wifiPending = wifi?.state === "connecting";
      const ethOnline = local?.ethernet?.online === true ||
        (local?.ethernet?.linkUp === true && local?.ethernet?.hasIp === true);
      const ethPending = local?.ethernet?.initialized === true && !ethOnline;
      const bleOnline = local?.ble?.linkConnected === true || local?.ble?.connected === true;
      const cloudOnline = cloud?.connected === true;
      const cloudPending = cloud?.configured === true && !cloudOnline;

      const wifiState = document.getElementById("commWifiState");
      const ethState = document.getElementById("commEthState");
      const bleState = document.getElementById("commBleState");
      const cloudState = document.getElementById("commCloudState");
      if (wifiState) wifiState.innerHTML = connectivityBadge(wifiOnline, wifiPending);
      if (ethState) ethState.innerHTML = connectivityBadge(ethOnline, ethPending);
      if (bleState) bleState.innerHTML = connectivityBadge(bleOnline, false);
      if (cloudState) cloudState.innerHTML = connectivityBadge(cloudOnline, cloudPending);

      const wifiIp = document.getElementById("commWifiIp");
      const ethIp = document.getElementById("commEthIp");
      const bleIp = document.getElementById("commBleIp");
      const cloudIp = document.getElementById("commCloudIp");
      if (wifiIp) wifiIp.textContent = local?.wifi?.ip || wifi?.ip || "—";
      if (ethIp) ethIp.textContent = local?.ethernet?.ip || "—";
      if (bleIp) bleIp.textContent = "—";
      if (cloudIp) cloudIp.textContent = cloud?.deviceId || "—";

      const updated = document.getElementById("adminConnectivityUpdated");
      if (updated) updated.textContent = `Оновлено ${new Date().toLocaleTimeString("uk-UA")}`;
      matchConnectivityHeight();
    } catch (error) {
      const updated = document.getElementById("adminConnectivityUpdated");
      if (updated) updated.textContent = `Помилка: ${error.message}`;
    }
  }

  function zoneView(raw) {
    const value = Number(raw);
    if (value === 0) return { label: "НОРМА", cls: "hg-zone-normal" };
    if (value === 1) return { label: "ОБРИВ", cls: "hg-zone-open" };
    if (value === 4) return { label: "КЗ", cls: "hg-zone-short" };
    return { label: "—", cls: "hg-zone-unavailable" };
  }

  function zoneStateFromMv(raw) {
    const millivolts = Number(raw);
    if (!Number.isFinite(millivolts)) return 3;
    if (millivolts <= 350) return 4;
    if (millivolts >= 2950) return 1;
    if (millivolts >= 1250 && millivolts <= 2050) return 0;
    return millivolts < 1250 ? 4 : 1;
  }

  function renderLiveZones(states, millivolts = null) {
    if (!Array.isArray(states) || states.length < 8) return;
    live.zones = states.slice(0, 8).map(Number);
    live.zonesMv = Array.isArray(millivolts) ? millivolts.slice(0, 8).map(Number) : live.zonesMv;
    live.zonesAt = Date.now();
    const target = document.querySelector("#zones");
    if (!target) return;
    const count = document.querySelector("#zoneCount");
    if (count) count.textContent = "8";
    target.innerHTML = live.zones.map((state, index) => {
      const meta = live.zoneMeta[index] || {};
      const view = zoneView(state);
      const name = meta.name || `Zone ${index + 1}`;
      const mv = live.zonesMv?.[index];
      const title = Number.isFinite(mv) ? ` title="${(mv / 1000).toFixed(3)} V"` : "";
      return `<div class="zone ${view.cls}" data-zone-id="${index + 1}" data-zone-state="${state}"${title}><span>${escape(name)}${meta.alwaysOn ? " · 24/7" : ""}</span><strong>${view.label}</strong></div>`;
    }).join("");
  }

  const originalRenderZones = typeof renderZones === "function" ? renderZones : null;
  if (originalRenderZones) {
    renderZones = function(data) {
      if (Array.isArray(data?.zones)) live.zoneMeta = data.zones.slice(0, 8);
      if (live.zones && Date.now() - live.zonesAt < 3500) renderLiveZones(live.zones, live.zonesMv);
      else originalRenderZones(data);
    };
  }

  function setQuickState(id, active) {
    const button = document.getElementById(id);
    if (!button) return;
    button.dataset.active = active ? "true" : "false";
    button.setAttribute("aria-pressed", active ? "true" : "false");
    const small = button.querySelector("small");
    if (!small) return;
    if (id === "quickLight") small.textContent = active ? "УВІМКНЕНО" : "ВИМКНЕНО";
    else small.textContent = active ? "ВІДКРИТО" : "ЗАКРИТО";
  }

  function syncOutputSnapshot(data) {
    const outputs = Array.isArray(data?.outputs) ? data.outputs : [];
    live.outputs = new Map(outputs.map(item => [Number(item.id), item.active === true]));
    if (live.outputs.has(4)) setQuickState("quickLight", live.outputs.get(4));
    if (live.outputs.has(5)) setQuickState("quickLock", live.outputs.get(5));
  }

  const originalRenderOutputs = typeof renderOutputs === "function" ? renderOutputs : null;
  if (originalRenderOutputs) {
    renderOutputs = function(data) {
      originalRenderOutputs(data);
      syncOutputSnapshot(data);
    };
  }

  async function refreshOutputs() {
    if (!window.HomeGuardAuth?.authenticated?.()) return;
    try {
      const data = await api("/api/v1/system/outputs");
      syncOutputSnapshot(data);
    } catch (_) {
    }
  }

  async function refreshAnalogZones() {
    if (!window.HomeGuardAuth?.authenticated?.()) return;
    try {
      const data = await api("/api/v1/hardware/analog");
      const devices = Array.isArray(data?.devices) ? data.devices : [];
      const first = devices.find(item => item?.role === "zones" || Number(item?.address) === 0x48);
      const second = devices.find(item => item?.role === "telemetry" || Number(item?.address) === 0x49);
      const firstValues = Array.isArray(first?.channels_mv) ? first.channels_mv : [];
      const secondValues = Array.isArray(second?.channels_mv) ? second.channels_mv : [];
      const values = [...firstValues.slice(0, 4), ...secondValues.slice(0, 4)];
      if (values.length !== 8 || values.some(value => !Number.isFinite(Number(value)))) return;
      const mv = values.map(Number);
      renderLiveZones(mv.map(zoneStateFromMv), mv);
    } catch (_) {
    }
  }

  async function commandOutput(outputId, active) {
    const actor = requireSessionActor();
    await api("/api/v1/system/output-command", {
      method: "POST",
      body: JSON.stringify({ outputId, active, actor })
    });
    await refreshOutputs();
  }

  async function commandSecurity(command) {
    const actor = requireSessionActor();
    await api("/api/v1/system/security-command", {
      method: "POST",
      body: JSON.stringify({ command, actor })
    });
    if (typeof refresh === "function") await refresh();
  }

  async function handleLight(button) {
    button.disabled = true;
    try {
      await refreshOutputs();
      const next = live.outputs.get(4) !== true;
      await commandOutput(4, next);
      setQuickState("quickLight", next);
      showToast(next ? "Світло увімкнено" : "Світло вимкнено");
    } catch (error) {
      showToast(`Світло: ${error.message}`);
    } finally {
      button.disabled = false;
    }
  }

  async function handleLock(button) {
    if (live.lockBusy) return;
    live.lockBusy = true;
    button.disabled = true;
    try {
      await commandOutput(5, true);
      setQuickState("quickLock", true);
      showToast("Замок відкрито на 5 секунд");
      await new Promise(resolve => window.setTimeout(resolve, 5000));
      await commandOutput(5, false);
      setQuickState("quickLock", false);
      showToast("Замок закрито");
    } catch (error) {
      try { await commandOutput(5, false); } catch (_) {}
      setQuickState("quickLock", false);
      showToast(`Замок: ${error.message}`);
    } finally {
      live.lockBusy = false;
      button.disabled = false;
    }
  }

  async function handleSecurity(button) {
    if (live.commandBusy) return;
    const command = button.dataset.command || "";
    if (!command) return;
    live.commandBusy = true;
    const buttons = [...document.querySelectorAll(".quick [data-command]")];
    const previous = new Map(buttons.map(item => [item, item.disabled]));
    buttons.forEach(item => { item.disabled = true; });
    try {
      await commandSecurity(command);
      showToast("Команду виконано");
    } catch (error) {
      showToast(`Команда: ${error.message}`);
    } finally {
      buttons.forEach(item => { item.disabled = previous.get(item) === true; });
      live.commandBusy = false;
    }
  }

  async function handleOutputButton(button) {
    const outputId = Number(button.dataset.outputId);
    const active = button.dataset.outputActive === "true";
    if (!Number.isInteger(outputId) || outputId <= 0) return;
    const buttons = [...document.querySelectorAll("#ioState [data-output-id]")];
    buttons.forEach(item => { item.disabled = true; });
    try {
      await commandOutput(outputId, active);
      showToast(active ? "Вихід увімкнено" : "Вихід вимкнено");
      if (typeof refresh === "function") await refresh();
    } catch (error) {
      showToast(`Вихід: ${error.message}`);
    }
  }

  document.addEventListener("click", async event => {
    const button = event.target.closest?.("#quickLight,#quickLock,.quick [data-command],#ioState [data-output-id]");
    if (!button) return;
    event.preventDefault();
    event.stopImmediatePropagation();

    if (!window.HomeGuardAuth?.authenticated?.()) {
      showToast("Потрібен активний сеанс");
      return;
    }

    if (button.id === "quickLight") return handleLight(button);
    if (button.id === "quickLock") return handleLock(button);
    if (button.matches(".quick [data-command]")) return handleSecurity(button);
    if (button.matches("#ioState [data-output-id]")) return handleOutputButton(button);
  }, true);

  function matchConnectivityHeight() {
    const card = document.getElementById("adminConnectivityCard");
    const zonesCard = document.getElementById("zones-section");
    if (!card || !zonesCard) return;
    if (window.innerWidth <= 1100) {
      card.style.removeProperty("height");
      card.style.removeProperty("min-height");
      card.style.removeProperty("max-height");
      return;
    }
    const height = Math.round(zonesCard.getBoundingClientRect().height);
    if (height <= 0) return;
    card.style.setProperty("height", `${height}px`, "important");
    card.style.setProperty("min-height", `${height}px`, "important");
    card.style.setProperty("max-height", `${height}px`, "important");
    card.style.setProperty("padding", "10px 14px", "important");
    card.style.setProperty("overflow", "hidden", "important");
  }

  function startWhenAuthenticated() {
    window.clearInterval(live.authTimer);
    live.authTimer = window.setInterval(() => {
      if (!window.HomeGuardAuth?.authenticated?.()) return;
      window.clearInterval(live.authTimer);
      live.authTimer = 0;
      refreshOutputs();
      refreshAnalogZones();
      ensureAdminConnectivityCard();
      refreshAdminConnectivity();
      if (!live.outputTimer) live.outputTimer = window.setInterval(refreshOutputs, 1000);
      if (!live.analogTimer) live.analogTimer = window.setInterval(refreshAnalogZones, 1000);
      if (window.HomeGuardAuth?.role?.() === "admin" && !live.connectivityTimer) {
        live.connectivityTimer = window.setInterval(refreshAdminConnectivity, 5000);
      }
    }, 200);
  }

  ensureSessionQuickControls();
  const observer = new MutationObserver(() => {
    ensureSessionQuickControls();
    if (window.HomeGuardAuth?.authenticated?.() && window.HomeGuardAuth?.role?.() === "admin") {
      ensureAdminConnectivityCard();
    }
    matchConnectivityHeight();
  });
  observer.observe(document.documentElement, { childList: true, subtree: true });
  window.addEventListener("resize", matchConnectivityHeight);
  window.addEventListener("load", matchConnectivityHeight);
  startWhenAuthenticated();

  window.HomeGuardLiveDebug = {
    zoneMillivolts: () => live.zonesMv ? [...live.zonesMv] : null,
    zoneStates: () => live.zones ? [...live.zones] : null,
    outputs: () => Object.fromEntries(live.outputs),
    refreshAnalogZones,
    refreshOutputs,
    refreshAdminConnectivity,
  };
})();
