"use strict";

(() => {
  const POLL_MS = 5000;
  const LAN_POLL_MS = 15000;

  function elements() {
    return { state: document.querySelector("#cloudState"), detail: document.querySelector("#cloudDetail") };
  }

  function escapeHtml(value) {
    return String(value ?? "")
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/\"/g, "&quot;")
      .replace(/'/g, "&#39;");
  }

  function paint(label, detail, tone) {
    const { state, detail: detailNode } = elements();
    if (!state || !detailNode) return;
    state.textContent = label; detailNode.textContent = detail;
    state.classList.remove("green-text", "orange-text", "red", "blue-text");
    if (tone) state.classList.add(tone);
  }

  function renderCloudStatus(status) {
    const configured = status?.configured === true;
    const connected = status?.connected === true;
    const deviceId = typeof status?.device_id === "string" && status.device_id ? status.device_id : "—";
    const connects = Number.isFinite(Number(status?.connect_count)) ? Number(status.connect_count) : 0;
    const disconnects = Number.isFinite(Number(status?.disconnect_count)) ? Number(status.disconnect_count) : 0;
    if (connected) { paint("Підключено", `${deviceId} · MQTT online · з'єднань ${connects}`, "green-text"); return; }
    if (configured) { paint("Немає зв'язку", `${deviceId} · повторне підключення · відключень ${disconnects}`, "orange-text"); return; }
    paint("Не налаштовано", `${deviceId} · MQTT не налаштовано`, "orange-text");
  }

  async function refreshCloudStatus() {
    try {
      const response = await fetch("/api/v1/cloud/status", { cache: "no-store" });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const status = await response.json(); renderCloudStatus(status);
      document.documentElement.dataset.homeguardCloudStatus = status?.connected === true ? "connected" : "disconnected";
    } catch (_) {
      paint("Недоступно", "Статус MQTT не отримано", "red");
      document.documentElement.dataset.homeguardCloudStatus = "unavailable";
    }
  }

  function ensureLanPanel() {
    const page = document.querySelector("#networkPage");
    if (!page || document.querySelector("#lanDiscoveryPanel")) return;
    const panel = document.createElement("article"); panel.id = "lanDiscoveryPanel"; panel.className = "panel"; panel.style.cssText = "max-width:920px;margin-top:16px";
    panel.innerHTML = `<h3>Пристрої локальної мережі (LAN)</h3><div style="display:flex;gap:10px;align-items:center;flex-wrap:wrap;margin:12px 0"><button id="lanScan" type="button">Оновити пристрої</button><span id="lanScanState">Автомоніторинг кожні 15 с</span></div><div style="overflow:auto"><table style="width:100%;border-collapse:collapse"><thead><tr><th style="text-align:left;padding:8px">Стан</th><th style="text-align:left;padding:8px">IP</th><th style="text-align:left;padding:8px">MAC</th><th style="text-align:left;padding:8px">Ім'я</th></tr></thead><tbody id="lanDevices"><tr><td colspan="4" style="padding:8px">Дані ще не отримані</td></tr></tbody></table></div>`;
    page.appendChild(panel); panel.querySelector("#lanScan").addEventListener("click", () => scanLan(false));
  }

  async function scanLan(silent = false) {
    const button = document.querySelector("#lanScan"), state = document.querySelector("#lanScanState"), target = document.querySelector("#lanDevices");
    if (!button || !state || !target) return;
    button.disabled = true;
    if (!silent) state.textContent = "Оновлення LAN…";
    try {
      const response = await fetch("/api/v1/network/lan-scan", { cache: "no-store" });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const payload = await response.json(), devices = Array.isArray(payload?.devices) ? payload.devices : [];
      state.textContent = `${devices.length} пристроїв · оновлено ${new Date().toLocaleTimeString("uk-UA")}`;
      target.innerHTML = devices.length ? devices.map(device => `<tr><td style="padding:8px">${device.online === false ? "Offline" : "Online"}</td><td style="padding:8px">${escapeHtml(device.ip || "—")}</td><td style="padding:8px">${escapeHtml(device.mac || "—")}</td><td style="padding:8px">${escapeHtml(device.hostname || "—")}</td></tr>`).join("") : '<tr><td colspan="4" style="padding:8px">Активних сусідів у ARP-таблиці не знайдено</td></tr>';
      document.documentElement.dataset.homeguardLanDevices = String(devices.length);
    } catch (error) {
      state.textContent = `Помилка LAN monitor: ${error.message}`; target.innerHTML = '<tr><td colspan="4" style="padding:8px">LAN discovery недоступний</td></tr>';
      document.documentElement.dataset.homeguardLanDevices = "error";
    } finally { button.disabled = false; }
  }

  function monitorLan() {
    const page = document.querySelector("#networkPage");
    if (page && !page.hidden) scanLan(true);
  }

  function loadConfigUi() {
    if (document.querySelector('script[data-homeguard-config-ui]')) return;
    const script = document.createElement("script"); script.src = "/config-ui.js?v=0060"; script.dataset.homeguardConfigUi = "1"; document.body.appendChild(script);
  }

  window.homeguardRefreshCloudStatus = refreshCloudStatus; window.homeguardScanLan = () => scanLan(false);
  ensureLanPanel(); loadConfigUi(); refreshCloudStatus(); setInterval(refreshCloudStatus, POLL_MS); setInterval(monitorLan, LAN_POLL_MS);
})();
