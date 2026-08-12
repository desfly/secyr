"use strict";

(() => {
  const INTERVAL_MS = 15000;
  let timer = null;
  let busy = false;

  function escapeHtml(value) {
    return String(value ?? "")
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/\"/g, "&quot;")
      .replace(/'/g, "&#39;");
  }

  function networkPageVisible() {
    const page = document.querySelector("#networkPage");
    return !!page && !page.hidden;
  }

  function render(devices) {
    const body = document.querySelector("#lanDevices");
    const count = document.querySelector("#lanDeviceCount");
    if (!body || !count) return;

    count.textContent = String(devices.length);
    body.innerHTML = devices.length ? devices.map(device => `
      <tr>
        <td>${device.online === false ? "Ні" : "Так"}</td>
        <td>${escapeHtml(device.ip || "—")}</td>
        <td>${escapeHtml(device.mac || "—")}</td>
        <td>${escapeHtml(device.hostname || "—")}</td>
      </tr>`).join("") : '<tr><td colspan="4">Активних пристроїв у ARP-таблиці не знайдено</td></tr>';
  }

  async function refreshLanDevices() {
    if (busy) return;
    const status = document.querySelector("#lanScanState");
    const button = document.querySelector("#lanRefresh");
    if (!status) return;

    busy = true;
    if (button) button.disabled = true;
    status.textContent = "Оновлення…";
    try {
      const response = await fetch("/api/v1/network/lan-scan", { cache: "no-store" });
      const payload = await response.json().catch(() => ({}));
      if (!response.ok || payload.ok === false) throw new Error(payload.reason || `HTTP ${response.status}`);
      const devices = Array.isArray(payload.devices) ? payload.devices : [];
      render(devices);
      status.textContent = `Оновлено ${new Date().toLocaleTimeString("uk-UA")} · метод ${payload.method || "arp"}`;
    } catch (error) {
      status.textContent = `Помилка LAN: ${error.message}`;
    } finally {
      busy = false;
      if (button) button.disabled = false;
    }
  }

  function schedule() {
    clearInterval(timer);
    timer = setInterval(() => {
      if (networkPageVisible()) refreshLanDevices();
    }, INTERVAL_MS);
  }

  document.addEventListener("DOMContentLoaded", () => {
    const button = document.querySelector("#lanRefresh");
    if (button) button.addEventListener("click", refreshLanDevices);
    schedule();
    if (networkPageVisible()) refreshLanDevices();
  });

  window.addEventListener("hashchange", () => {
    if (networkPageVisible()) refreshLanDevices();
  });

  window.HomeGuardLanMonitor = { refresh: refreshLanDevices };
})();
