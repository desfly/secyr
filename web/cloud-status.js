"use strict";

(() => {
  const POLL_MS = 5000;

  function elements() {
    return {
      state: document.querySelector("#cloudState"),
      detail: document.querySelector("#cloudDetail"),
    };
  }

  function paint(label, detail, tone) {
    const { state, detail: detailNode } = elements();
    if (!state || !detailNode) return;
    state.textContent = label;
    detailNode.textContent = detail;
    state.classList.remove("green-text", "orange-text", "red", "blue-text");
    if (tone) state.classList.add(tone);
  }

  function renderCloudStatus(status) {
    const configured = status?.configured === true;
    const connected = status?.connected === true;
    const deviceId = typeof status?.device_id === "string" && status.device_id ? status.device_id : "—";
    const connects = Number.isFinite(Number(status?.connect_count)) ? Number(status.connect_count) : 0;
    const disconnects = Number.isFinite(Number(status?.disconnect_count)) ? Number(status.disconnect_count) : 0;

    if (connected) {
      paint("Підключено", `${deviceId} · MQTT online · з'єднань ${connects}`, "green-text");
      return;
    }
    if (configured) {
      paint("Немає зв'язку", `${deviceId} · повторне підключення · відключень ${disconnects}`, "orange-text");
      return;
    }
    paint("Не налаштовано", `${deviceId} · MQTT не налаштовано`, "orange-text");
  }

  async function refreshCloudStatus() {
    try {
      const response = await fetch("/api/v1/cloud/status", { cache: "no-store" });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const status = await response.json();
      renderCloudStatus(status);
      document.documentElement.dataset.homeguardCloudStatus = status?.connected === true ? "connected" : "disconnected";
    } catch (error) {
      paint("Недоступно", "Статус MQTT не отримано", "red");
      document.documentElement.dataset.homeguardCloudStatus = "unavailable";
    }
  }

  window.homeguardRefreshCloudStatus = refreshCloudStatus;
  refreshCloudStatus();
  setInterval(refreshCloudStatus, POLL_MS);
})();
