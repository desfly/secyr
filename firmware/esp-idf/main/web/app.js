const actor = `web:${crypto.randomUUID()}`;
const toast = document.querySelector("#toast");
let selectedWifiSsid = "";
let networkPollTimer = null;

function showToast(message) {
  toast.textContent = message;
  toast.hidden = false;
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => toast.hidden = true, 2500);
}

async function api(path, options = {}) {
  const response = await fetch(path, {
    cache: "no-store",
    ...options,
    headers: {"Content-Type": "application/json", ...(options.headers || {})}
  });
  if (!response.ok) throw new Error(`${response.status} ${response.statusText}`);
  return response.json();
}

function stateClass(value) {
  if (["normal", "closed", "open", "ready", "disarmed"].includes(value)) return "ok";
  if (["alarm", "fault", "jammed", "timeout", "open_circuit", "short_circuit"].includes(value)) return "alarm";
  return "warning";
}

function ukSecurityMode(value) {
  const map = {disarmed:"ЗНЯТО", arm_away:"ОХОРОНА", armed_away:"ОХОРОНА", arm_home:"НІЧНИЙ", armed_home:"НІЧНИЙ"};
  return map[value] || value || "—";
}

function ukZoneState(value) {
  const map = {normal:"Норма", open:"Відкрито", alarm:"Тривога", tamper:"Тампер", fault:"Помилка", bypassed:"Обхід"};
  return map[value] || value || "—";
}

function renderSystem(state) {
  document.querySelector("#securityMode").textContent = ukSecurityMode(state.security_mode);
  document.querySelector("#securitySub").textContent = state.security_mode ? "Реальний стан контролера" : "Стан недоступний";
  const zones = Array.isArray(state.zones) ? state.zones : [];
  document.querySelector("#zoneCount").textContent = `${zones.length}`;
  document.querySelector("#zoneSub").textContent = zones.length ? "Зареєстровано" : "Зони не налаштовані";
  document.querySelector("#coldPressure").textContent = `${Number(state.cold_pressure_bar || 0).toFixed(2)} бар`;
  document.querySelector("#hotPressure").textContent = `${Number(state.hot_pressure_bar || 0).toFixed(2)} бар`;
  document.querySelector("#coldTemperature").textContent = `${Number(state.cold_temperature_c || 0).toFixed(1)} °C`;
  document.querySelector("#hotTemperature").textContent = `${Number(state.hot_temperature_c || 0).toFixed(1)} °C`;
  document.querySelector("#mains").textContent = state.mains_present ? "Є" : "Немає";
  document.querySelector("#battery").textContent = `${Number(state.battery_voltage_v || 0).toFixed(2)} V / ${Number(state.battery_current_a || 0).toFixed(2)} A`;
  document.querySelector("#light").textContent = state.corridor_light ? "Увімкнено" : "Вимкнено";

  document.querySelector("#zones").innerHTML = zones.length ? zones.map(zone => `
    <div class="zone"><span>${zone.title || zone.name || `Зона ${zone.id}`}${zone.always_on ? " · 24/7" : ""}</span><strong class="${stateClass(zone.state)}">${ukZoneState(zone.state)}</strong></div>
  `).join("") : `<div class="zone"><span>Зони не налаштовані</span><strong class="warning">—</strong></div>`;

  const valves = Array.isArray(state.valves) ? state.valves : [];
  document.querySelector("#valves").innerHTML = valves.map(valve => `<div class="valve"><span>${valve.id}</span><strong class="${stateClass(valve.state)}">${valve.state}</strong></div>`).join("");

  const io = [];
  if (Array.isArray(state.inputs)) state.inputs.forEach((item, i) => io.push(`<div><b>▯</b><span>Вх. ${i+1}</span><small class="${stateClass(item.state)}">${ukZoneState(item.state)}</small></div>`));
  if (Array.isArray(state.outputs)) state.outputs.forEach((item, i) => io.push(`<div><b>⇆</b><span>Вих. ${i+1}</span><small>${item.active ? "Увімк." : "Вимк."}</small></div>`));
  document.querySelector("#ioState").innerHTML = io.length ? io.join("") : `<div><span>Входи/виходи ще не опубліковані API</span></div>`;
  bindCommandButtons();
}

function setNetworkMessage(text, type = "") {
  const el = document.querySelector("#networkMessage");
  el.textContent = text;
  el.className = `network-message ${type}`.trim();
}

function renderWifi(wifi) {
  const connected = wifi.station === "connected";
  const connecting = wifi.station === "connecting";
  const ssid = wifi.station_ssid || (wifi.softap ? wifi.ssid : "Не налаштовано");
  const ip = connected ? (wifi.station_ip || "—") : (wifi.softap ? (wifi.ip || "192.168.4.1") : "—");
  const stateText = connected ? "Підключено" : connecting ? "Підключення…" : (wifi.softap ? "Точка налаштування активна" : "Не підключено");

  document.querySelector("#wifiName").textContent = ssid;
  document.querySelector("#connection").textContent = connected ? ip : connecting ? "Підключення…" : (wifi.softap ? `AP ${ip}` : "Не налаштовано");
  document.querySelector("#networkState").textContent = stateText;
  document.querySelector("#networkSsid").textContent = ssid;
  document.querySelector("#networkIp").textContent = ip;

  if (connected) setNetworkMessage(`Підключено до ${wifi.station_ssid || "Wi-Fi"}. IP: ${wifi.station_ip || "—"}`, "ok");
  else if (connecting) setNetworkMessage("Підключення до Wi-Fi…", "wait");
}

function renderCloud(cloud) {
  const state = document.querySelector("#cloudState");
  const sub = document.querySelector("#cloudSub");
  document.querySelector("#deviceId").textContent = cloud.device_id || "HomeGuard-S3";
  if (cloud.connected) {
    state.textContent = "Підключено"; state.className = "status-ok"; sub.textContent = "MQTT online";
  } else if (cloud.configured) {
    state.textContent = "Немає зв'язку"; state.className = "status-error"; sub.textContent = "MQTT налаштовано";
  } else {
    state.textContent = "Не налаштовано"; state.className = "status-unconfigured"; sub.textContent = "MQTT вимкнено";
  }
}

async function refreshWifiOnly() {
  try {
    const wifi = await api("/api/v1/wifi/status");
    renderWifi(wifi);
    return wifi;
  } catch (error) {
    document.querySelector("#networkState").textContent = "Помилка";
    setNetworkMessage(`Помилка отримання стану: ${error.message}`, "error");
    throw error;
  }
}

async function scanWifi() {
  const button = document.querySelector("#scanWifi");
  const list = document.querySelector("#wifiNetworks");
  button.disabled = true;
  button.textContent = "Сканування…";
  list.innerHTML = `<div class="wifi-row"><strong>Сканування Wi-Fi…</strong><small>Зачекайте</small><span></span></div>`;
  setNetworkMessage("Пошук доступних мереж…", "wait");
  try {
    const result = await api("/api/v1/wifi/scan");
    const networks = Array.isArray(result.networks) ? result.networks.filter(n => n.ssid) : [];
    const unique = [];
    const seen = new Set();
    networks.sort((a,b) => Number(b.rssi || -100) - Number(a.rssi || -100)).forEach(n => { if (!seen.has(n.ssid)) { seen.add(n.ssid); unique.push(n); } });
    if (!unique.length) {
      list.innerHTML = `<div class="wifi-row"><strong>Мереж не знайдено</strong><small>Повторіть сканування</small><span></span></div>`;
      setNetworkMessage("Мереж не знайдено", "error");
      return;
    }
    list.innerHTML = unique.map(n => `<button type="button" class="wifi-row" data-ssid="${escapeHtml(n.ssid)}"><strong>${escapeHtml(n.ssid)}</strong><span class="wifi-rssi">${Number(n.rssi)} dBm</span><small>Канал ${Number(n.channel || 0)}</small></button>`).join("");
    list.querySelectorAll(".wifi-row[data-ssid]").forEach(row => row.onclick = () => selectWifi(row));
    setNetworkMessage(`Знайдено мереж: ${unique.length}`, "ok");
  } catch (error) {
    list.innerHTML = `<div class="wifi-row"><strong>Помилка сканування</strong><small>${escapeHtml(error.message)}</small><span></span></div>`;
    setNetworkMessage(`Помилка сканування: ${error.message}`, "error");
  } finally {
    button.disabled = false;
    button.textContent = "Сканувати Wi-Fi";
  }
}

function escapeHtml(value) {
  return String(value ?? "").replace(/[&<>'"]/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;","'":"&#39;",'"':"&quot;"}[c]));
}

function selectWifi(row) {
  document.querySelectorAll(".wifi-row.selected").forEach(el => el.classList.remove("selected"));
  row.classList.add("selected");
  selectedWifiSsid = row.dataset.ssid || "";
  document.querySelector("#wifiSsid").value = selectedWifiSsid;
  document.querySelector("#wifiPassword").focus();
  setNetworkMessage(`Обрано мережу: ${selectedWifiSsid}`);
}

async function connectWifi() {
  const button = document.querySelector("#connectWifi");
  const ssid = document.querySelector("#wifiSsid").value.trim();
  const password = document.querySelector("#wifiPassword").value;
  if (!ssid) { setNetworkMessage("Оберіть або введіть SSID", "error"); return; }
  if (ssid.length > 32) { setNetworkMessage("SSID задовгий", "error"); return; }
  if (password.length > 64) { setNetworkMessage("Пароль задовгий", "error"); return; }

  button.disabled = true;
  button.textContent = "Підключення…";
  setNetworkMessage(`Передаю налаштування для ${ssid}…`, "wait");
  try {
    const response = await api("/api/v1/provisioning/wifi", {method:"POST", body:JSON.stringify({ssid,password})});
    if (!response.accepted) throw new Error("контролер відхилив налаштування");
    setNetworkMessage("Підключення… очікую IP-адресу", "wait");
    startNetworkPolling(ssid);
  } catch (error) {
    setNetworkMessage(`Помилка: ${error.message}`, "error");
    button.disabled = false;
    button.textContent = "Підключити";
  }
}

function startNetworkPolling(expectedSsid) {
  clearInterval(networkPollTimer);
  let attempts = 0;
  networkPollTimer = setInterval(async () => {
    attempts += 1;
    try {
      const wifi = await refreshWifiOnly();
      if (wifi.station === "connected") {
        clearInterval(networkPollTimer);
        networkPollTimer = null;
        document.querySelector("#connectWifi").disabled = false;
        document.querySelector("#connectWifi").textContent = "Підключити";
        setNetworkMessage(`Підключено до ${wifi.station_ssid || expectedSsid}. IP: ${wifi.station_ip || "—"}`, "ok");
      } else if (attempts >= 20) {
        clearInterval(networkPollTimer);
        networkPollTimer = null;
        document.querySelector("#connectWifi").disabled = false;
        document.querySelector("#connectWifi").textContent = "Підключити";
        setNetworkMessage("Помилка або таймаут підключення. Перевірте пароль і спробуйте ще раз.", "error");
      }
    } catch (_) {
      if (attempts >= 20) {
        clearInterval(networkPollTimer);
        networkPollTimer = null;
        document.querySelector("#connectWifi").disabled = false;
        document.querySelector("#connectWifi").textContent = "Підключити";
      }
    }
  }, 1500);
}

function showNetworkScreen() {
  document.querySelector("#overview").style.display = "none";
  document.querySelector("#network-screen").classList.add("active");
  document.querySelectorAll(".sidebar nav a").forEach(a => a.classList.remove("active"));
  document.querySelector("#navNetwork").classList.add("active");
  refreshWifiOnly().catch(() => {});
}

function showDashboard() {
  document.querySelector("#network-screen").classList.remove("active");
  document.querySelector("#overview").style.display = "block";
  document.querySelectorAll(".sidebar nav a").forEach(a => a.classList.remove("active"));
  document.querySelector("#navOverview").classList.add("active");
}

function resetWifiForm() {
  selectedWifiSsid = "";
  document.querySelector("#wifiSsid").value = "";
  document.querySelector("#wifiPassword").value = "";
  document.querySelectorAll(".wifi-row.selected").forEach(el => el.classList.remove("selected"));
  setNetworkMessage("Оберіть іншу мережу та натисніть «Підключити»");
  scanWifi();
}

async function refresh() {
  const failures = [];
  const [stateR, buildR, wifiR, cloudR] = await Promise.allSettled([
    api("/api/v1/state"), api("/api/v1/build"), api("/api/v1/wifi/status"), api("/api/v1/cloud/status")
  ]);
  if (stateR.status === "fulfilled") renderSystem(stateR.value); else failures.push("state");
  if (buildR.status === "fulfilled") document.querySelector("#buildInfo").textContent = JSON.stringify(buildR.value, null, 2); else failures.push("build");
  if (wifiR.status === "fulfilled") renderWifi(wifiR.value); else failures.push("wifi");
  if (cloudR.status === "fulfilled") renderCloud(cloudR.value); else failures.push("cloud");
  document.querySelector("#deviceOnlineText").textContent = failures.length ? "Частково доступний" : "Онлайн";
  document.querySelector("#headerOnline").textContent = failures.length ? "Частково" : "Онлайн";
  if (failures.length) document.querySelector("#connection").textContent += ` · API: ${failures.join(", ")}`;
}

async function sendCommand(button) {
  button.disabled = true;
  try {
    const payload = {request_id: crypto.randomUUID(), actor, command: button.dataset.command, target: button.dataset.target || "", value: button.dataset.value || ""};
    const response = await api("/api/v1/command", {method:"POST", body:JSON.stringify(payload)});
    showToast(`${response.code}: ${response.message}`);
    await refresh();
  } catch (error) { showToast(`Помилка: ${error.message}`); }
  finally { button.disabled = false; }
}

function bindCommandButtons() { document.querySelectorAll("[data-command]").forEach(button => button.onclick = () => sendCommand(button)); }
document.querySelector("#panicButton").onclick = () => showToast("Паніка ще не підключена до API — команда не відправлена");
document.querySelector("#refresh").onclick = refresh;
document.querySelector("#navNetwork").onclick = event => { event.preventDefault(); showNetworkScreen(); };
document.querySelector("#navOverview").onclick = event => { event.preventDefault(); showDashboard(); };
document.querySelector("#backDashboard").onclick = showDashboard;
document.querySelector("#scanWifi").onclick = scanWifi;
document.querySelector("#connectWifi").onclick = connectWifi;
document.querySelector("#changeWifi").onclick = resetWifiForm;
bindCommandButtons();
refresh();
setInterval(refresh, 5000);
