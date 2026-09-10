"use strict";

/*
 * Direct relay dashboard runtime.
 *
 * This file is appended to the embedded app.js by the ESP-IDF CMake build.
 * It deliberately owns the Light and Lock dashboard buttons so legacy/generic
 * output handlers cannot route these two physical relays through
 * /api/v1/system/output-command (which is a different, interlocked output
 * model and can legitimately answer 409 Conflict).
 *
 * Canonical direct relay API:
 *   GET  /api/v1/outputs/relay-state
 *   POST /api/v1/outputs/light
 *   POST /api/v1/outputs/lock/pulse
 */
(() => {
  const RELAY_POLL_MS = 250;
  let relayState = null;
  let pollBusy = false;

  const byId = (id) => document.getElementById(id);
  const actor = () => window.HomeGuardAuth?.actor?.() || byId("operatorId")?.value?.trim() || "";
  const authenticated = () => window.HomeGuardAuth?.authenticated?.() === true;
  const admin = () => window.HomeGuardAuth?.role?.() === "admin";

  function toast(message) {
    if (typeof window.showToast === "function") {
      window.showToast(message);
      return;
    }
    const node = byId("toast");
    if (!node) return;
    node.textContent = message;
    node.hidden = false;
    clearTimeout(toast.timer);
    toast.timer = window.setTimeout(() => { node.hidden = true; }, 3000);
  }

  async function request(path, options = {}) {
    const response = await fetch(path, {
      cache: "no-store",
      ...options,
      headers: { "Content-Type": "application/json", ...(options.headers || {}) },
    });
    const text = await response.text();
    let body = {};
    try { body = text ? JSON.parse(text) : {}; } catch (_) {}
    if (!response.ok || body.ok === false) {
      throw new Error(body.reason || `${response.status} ${response.statusText}`);
    }
    return body;
  }

  function createButton(id, title) {
    const quick = document.querySelector(".quick");
    if (!quick) return null;
    const button = document.createElement("button");
    button.id = id;
    button.type = "button";
    button.innerHTML = `<b>○</b><strong>${title}</strong><small>—</small>`;
    quick.appendChild(button);
    return button;
  }

  function directButton(id, kind) {
    let old = byId(id);
    if (!old) return null;
    if (old.dataset.relayDirectShim === "1") return old;

    // Clone to discard any legacy onclick/addEventListener handlers that may
    // still point at /api/v1/system/output-command.
    const button = old.cloneNode(true);
    button.removeAttribute("data-output-id");
    button.removeAttribute("data-output-active");
    button.dataset.relayDirect = kind;
    button.dataset.relayDirectShim = "1";
    old.replaceWith(button);

    button.addEventListener("click", (event) => {
      event.preventDefault();
      event.stopPropagation();
      event.stopImmediatePropagation();
      if (kind === "light") void toggleLight(button);
      else void pulseLock(button);
    }, true);
    return button;
  }

  function installButtons() {
    // Remove the former extra lock-only button if an older runtime created it.
    byId("lockRelayButton")?.remove();

    if (!byId("lightRelayControl") && !byId("hgQuickLight")) {
      createButton("lightRelayControl", "Освітлення");
    }
    if (!byId("lockRelayControl") && !byId("hgQuickLock")) {
      createButton("lockRelayControl", "Замок");
    }

    directButton("lightRelayControl", "light");
    directButton("hgQuickLight", "light");
    directButton("lockRelayControl", "lock");
    directButton("hgQuickLock", "lock");
    document.documentElement.dataset.homeguardRelayPath = "direct-relay-api";
  }

  function setButtonVisual(button, on, detail, activeColor) {
    if (!button) return;
    button.disabled = !admin() || !relayState;
    button.setAttribute("aria-pressed", String(on));
    const icon = button.querySelector("b");
    const small = button.querySelector("small");
    if (icon) icon.textContent = on ? "●" : "○";
    if (small) small.textContent = detail;
    button.style.boxShadow = on ? `inset 0 0 0 2px ${activeColor}` : "";
  }

  function render(state) {
    relayState = state && state.ok !== false ? state : null;
    installButtons();

    const lightOn = relayState?.lightActive === true;
    const manual = relayState?.lightManual === true;
    const automatic = relayState?.lightAutomatic === true;
    const lightDetail = !relayState ? "Немає даних" : lightOn
      ? (automatic && !manual ? "АВТО ON · Z1/Z2" : manual ? (automatic ? "РУЧНЕ + АВТО ON" : "РУЧНЕ ON") : "ON")
      : "OFF";

    const lockOn = relayState?.lockActive === true;
    const seconds = Math.max(0, Math.ceil(Number(relayState?.lockRemainingMs || 0) / 1000));
    const lockDetail = !relayState ? "Немає даних" : (lockOn ? `ON · ${seconds} с` : "OFF");

    [byId("lightRelayControl"), byId("hgQuickLight")].forEach((button) =>
      setButtonVisual(button, lightOn, lightDetail, "#22c55e"));
    [byId("lockRelayControl"), byId("hgQuickLock")].forEach((button) =>
      setButtonVisual(button, lockOn, lockDetail, "#f59e0b"));

    const lightState = byId("hgQuickLightState");
    const lockState = byId("hgQuickLockState");
    if (lightState) lightState.textContent = lightDetail;
    if (lockState) lockState.textContent = lockDetail;
  }

  async function refreshRelayState() {
    if (pollBusy || !authenticated() || document.hidden) return;
    pollBusy = true;
    try {
      render(await request("/api/v1/outputs/relay-state"));
    } catch (_) {
      render(null);
    } finally {
      pollBusy = false;
    }
  }

  async function toggleLight(button) {
    if (!admin() || !relayState || button.disabled) return;
    button.disabled = true;
    try {
      const state = await request("/api/v1/outputs/light", {
        method: "POST",
        body: JSON.stringify({ actor: actor(), active: relayState.lightManual !== true }),
      });
      render(state);
      toast(state.lightActive ? "Освітлення ON" : "Освітлення OFF");
    } catch (error) {
      toast(`Освітлення: ${error.message}`);
    } finally {
      void refreshRelayState();
    }
  }

  async function pulseLock(button) {
    if (!admin() || button.disabled) return;
    button.disabled = true;
    try {
      const state = await request("/api/v1/outputs/lock/pulse", {
        method: "POST",
        body: JSON.stringify({ actor: actor() }),
      });
      render(state);
      toast("Замок ON · 5 секунд");
    } catch (error) {
      toast(`Замок: ${error.message}`);
    } finally {
      void refreshRelayState();
    }
  }

  installButtons();
  render(null);

  async function loop() {
    await refreshRelayState();
    window.setTimeout(loop, RELAY_POLL_MS);
  }
  window.setTimeout(loop, 100);
  window.addEventListener("focus", () => void refreshRelayState());
  document.addEventListener("visibilitychange", () => {
    if (!document.hidden) void refreshRelayState();
  });

  window.HomeGuardRelayRuntime = {
    refresh: refreshRelayState,
    path: "direct-relay-api",
  };
})();
