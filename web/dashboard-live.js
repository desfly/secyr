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
    lockBusy: false,
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
    if (millivolts >= 1250 && millivolts <= 1950) return 0;
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
    const actor = window.HomeGuardAuth?.actor?.() || "";
    if (!actor) throw new Error("немає активного сеансу");
    await api("/api/v1/system/output-command", {
      method: "POST",
      body: JSON.stringify({ outputId, active, actor })
    });
    await refreshOutputs();
  }

  document.addEventListener("click", async event => {
    const button = event.target.closest?.("#quickLight,#quickLock");
    if (!button) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    if (!window.HomeGuardAuth?.authenticated?.()) {
      showToast("Потрібен активний сеанс");
      return;
    }

    if (button.id === "quickLight") {
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
      return;
    }

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
      if (!live.outputTimer) live.outputTimer = window.setInterval(refreshOutputs, 1000);
      if (!live.analogTimer) live.analogTimer = window.setInterval(refreshAnalogZones, 1000);
    }, 200);
  }

  const observer = new MutationObserver(matchConnectivityHeight);
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
  };
})();
