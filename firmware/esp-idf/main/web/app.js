const actor = `web:${crypto.randomUUID()}`;
const toast = document.querySelector("#toast");

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
  document.querySelector("#zoneCount").textContent = `${state.zones.length}`;
  document.querySelector("#zoneSub").textContent = state.zones.length ? "Зареєстровано" : "Зони не налаштовані";
  document.querySelector("#coldPressure").textContent = `${Number(state.cold_pressure_bar || 0).toFixed(2)} бар`;
  document.querySelector("#hotPressure").textContent = `${Number(state.hot_pressure_bar || 0).toFixed(2)} бар`;
  document.querySelector("#coldTemperature").textContent = `${Number(state.cold_temperature_c || 0).toFixed(1)} °C`;
  document.querySelector("#hotTemperature").textContent = `${Number(state.hot_temperature_c || 0).toFixed(1)} °C`;
  document.querySelector("#mains").textContent = state.mains_present ? "Є" : "Немає";
  document.querySelector("#battery").textContent = `${Number(state.battery_voltage_v || 0).toFixed(2)} V / ${Number(state.battery_current_a || 0).toFixed(2)} A`;
  document.querySelector("#light").textContent = state.corridor_light ? "Увімкнено" : "Вимкнено";

  const zones = Array.isArray(state.zones) ? state.zones : [];
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

function renderWifi(wifi) {
  const connected = wifi.station === "connected";
  const connecting = wifi.station === "connecting";
  document.querySelector("#wifiName").textContent = wifi.station_ssid || (wifi.softap ? wifi.ssid : "Не налаштовано");
  document.querySelector("#connection").textContent = connected ? (wifi.station_ip || "Підключено") : connecting ? "Підключення…" : (wifi.softap ? `AP ${wifi.ip || "192.168.4.1"}` : "Не налаштовано");
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
bindCommandButtons();
refresh();
setInterval(refresh, 5000);
