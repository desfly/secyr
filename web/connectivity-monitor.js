"use strict";

;(() => {
  let timer = 0;

  const label = (online, pending = false) => pending ? "ПІДКЛЮЧЕННЯ" : (online ? "ОНЛАЙН" : "ОФЛАЙН");
  const badge = (online, pending = false) => {
    const text = label(online, pending);
    const background = pending ? "#fff4cc" : (online ? "#dcf8e8" : "#ffe3e3");
    const foreground = pending ? "#8a6500" : (online ? "#157347" : "#b42318");
    return `<strong style="display:inline-block;padding:5px 9px;border-radius:999px;background:${background};color:${foreground}">${text}</strong>`;
  };
  const escapeHtml = value => String(value ?? "—")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/\"/g, "&quot;");

  function channelCard(id, title) {
    return `
      <div style="border:1px solid #d7deea;border-radius:10px;padding:12px;background:#fff;min-width:0">
        <div style="display:flex;justify-content:space-between;gap:8px;align-items:center;margin-bottom:8px">
          <strong>${title}</strong><span id="${id}Preferred" style="font-size:11px;font-weight:700;color:#1c5d99"></span>
        </div>
        <div id="${id}State" style="margin-bottom:8px">—</div>
        <div id="${id}Address" style="font-size:13px;overflow-wrap:anywhere">—</div>
        <div id="${id}Params" style="font-size:12px;color:#66758b;margin-top:5px;line-height:1.45">—</div>
      </div>`;
  }

  function ensurePanel() {
    if (window.HomeGuardAuth?.role?.() !== "admin") {
      document.getElementById("networkConnectivityPanel")?.remove();
      return null;
    }
    const networkPage = document.getElementById("networkPage");
    if (!networkPage) return null;
    let panel = document.getElementById("networkConnectivityPanel");
    if (panel) return panel;

    panel = document.createElement("article");
    panel.id = "networkConnectivityPanel";
    panel.className = "panel";
    panel.style.cssText = "max-width:920px;margin-bottom:18px";
    panel.innerHTML = `
      <div style="display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:10px">
        <div><h3 style="margin:0">Монітор каналів зв'язку</h3><small>Ethernet · Wi‑Fi · Bluetooth BLE · Cloud MQTT</small></div>
        <small id="networkConnectivityUpdated">—</small>
      </div>
      <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(185px,1fr));gap:10px">
        ${channelCard("netmonWifi", "Wi‑Fi")}
        ${channelCard("netmonEth", "Ethernet W5500")}
        ${channelCard("netmonBle", "Bluetooth BLE")}
        ${channelCard("netmonCloud", "Cloud MQTT")}
      </div>`;
    networkPage.insertBefore(panel, networkPage.firstElementChild);
    return panel;
  }

  async function getJson(path) {
    const response = await fetch(path, {cache: "no-store"});
    const text = await response.text();
    let body = {};
    try { body = text ? JSON.parse(text) : {}; } catch (_) {}
    if (!response.ok || body.ok === false) throw new Error(body.reason || String(response.status));
    return body;
  }

  function setChannel(prefix, state, address, params, preferred) {
    const stateNode = document.getElementById(`${prefix}State`);
    const addressNode = document.getElementById(`${prefix}Address`);
    const paramsNode = document.getElementById(`${prefix}Params`);
    const preferredNode = document.getElementById(`${prefix}Preferred`);
    if (stateNode) stateNode.innerHTML = state;
    if (addressNode) addressNode.innerHTML = address;
    if (paramsNode) paramsNode.innerHTML = params;
    if (preferredNode) preferredNode.textContent = preferred ? "АКТИВНИЙ" : "";
  }

  function setDashboardConnectivity(wifi, local, cloud) {
    const card = document.getElementById("adminConnectivityCard");
    if (!card) return;

    const wifiOnline = wifi?.state === "connected" || local?.wifi?.online === true;
    const wifiPending = wifi?.state === "connecting";
    const ethOnline = local?.ethernet?.online === true ||
      (local?.ethernet?.linkUp === true && local?.ethernet?.hasIp === true);
    const ethPending = local?.ethernet?.initialized === true && !ethOnline;
    const bleOnline = local?.ble?.linkConnected === true || local?.ble?.connected === true;
    const cloudOnline = cloud?.connected === true;
    const cloudPending = cloud?.configured === true && !cloudOnline;

    const states = [
      ["commWifiState", wifiOnline, wifiPending],
      ["commEthState", ethOnline, ethPending],
      ["commBleState", bleOnline, false],
      ["commCloudState", cloudOnline, cloudPending],
    ];
    states.forEach(([id, online, pending]) => {
      const node = document.getElementById(id);
      if (node) node.innerHTML = badge(online, pending);
    });

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
  }

  async function refreshConnectivityMonitor() {
    if (!window.HomeGuardAuth?.authenticated?.() || window.HomeGuardAuth?.role?.() !== "admin") return;
    ensurePanel();
    try {
      const [wifi, local, cloud] = await Promise.all([
        getJson("/api/v1/network/status"),
        getJson("/api/v1/connectivity/status"),
        getJson("/api/v1/cloud/status")
      ]);
      const preferred = String(local?.preferred || "offline");

      const wifiOnline = wifi?.state === "connected" || local?.wifi?.online === true;
      const wifiPending = wifi?.state === "connecting";
      const wifiSsid = local?.wifi?.ssid || wifi?.ssid || "—";
      const wifiIp = local?.wifi?.ip || wifi?.ip || "—";
      const wifiRssi = Number(local?.wifi?.rssi || 0);
      setChannel(
        "netmonWifi",
        badge(wifiOnline, wifiPending),
        `<strong>${escapeHtml(wifiIp)}</strong>`,
        `SSID: ${escapeHtml(wifiSsid)}<br>RSSI: ${wifiRssi ? `${wifiRssi} dBm` : "—"}`,
        preferred === "wifi");

      const ethOnline = local?.ethernet?.online === true || (local?.ethernet?.linkUp === true && local?.ethernet?.hasIp === true);
      const ethPending = local?.ethernet?.initialized === true && !ethOnline;
      setChannel(
        "netmonEth",
        badge(ethOnline, ethPending),
        `<strong>${escapeHtml(local?.ethernet?.ip || "—")}</strong>`,
        `Link: ${local?.ethernet?.linkUp === true ? "UP" : "DOWN"}<br>IP: ${local?.ethernet?.hasIp === true ? "отримано" : "немає"}`,
        preferred === "ethernet");

      const bleLink = local?.ble?.linkConnected === true || local?.ble?.connected === true;
      const bleNotify = local?.ble?.notificationsEnabled === true;
      const bleEpoch = Number(local?.ble?.connectionEpoch) || 0;
      setChannel(
        "netmonBle",
        badge(bleLink, false),
        "<strong>GATT / без IP</strong>",
        `Link: ${bleLink ? "CONNECTED" : "DISCONNECTED"}<br>Notify TX: ${bleNotify ? "ON" : "OFF"}<br>Session epoch: ${bleEpoch}`,
        preferred === "ble");

      const cloudOnline = cloud?.connected === true;
      const cloudPending = cloud?.configured === true && !cloudOnline;
      const cloudActive = preferred === "offline" && cloudOnline;
      setChannel(
        "netmonCloud",
        badge(cloudOnline, cloudPending),
        `<strong>${escapeHtml(cloud?.deviceId || "—")}</strong>`,
        `State: ${escapeHtml(cloud?.state || "disabled")}<br>Connect: ${Number(cloud?.connectCount) || 0}<br>Disconnect: ${Number(cloud?.disconnectCount) || 0}`,
        cloudActive);

      setDashboardConnectivity(wifi, local, cloud);

      const updated = document.getElementById("networkConnectivityUpdated");
      if (updated) updated.textContent = `Оновлено ${new Date().toLocaleTimeString("uk-UA")}`;
    } catch (error) {
      const updated = document.getElementById("networkConnectivityUpdated");
      if (updated) updated.textContent = `Помилка: ${error.message}`;
    }
  }

  const boot = window.setInterval(() => {
    if (!window.HomeGuardAuth?.authenticated?.()) return;
    window.clearInterval(boot);
    if (window.HomeGuardAuth?.role?.() !== "admin") return;
    ensurePanel();
    refreshConnectivityMonitor();
    timer = window.setInterval(refreshConnectivityMonitor, 2000);
  }, 250);

  window.addEventListener("hashchange", () => {
    refreshConnectivityMonitor();
  });

  window.HomeGuardConnectivityMonitor = {
    refresh: refreshConnectivityMonitor,
    stop() { if (timer) window.clearInterval(timer); timer = 0; }
  };
})();