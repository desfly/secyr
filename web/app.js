const toast = document.querySelector("#toast");

function showToast(message) {
  toast.textContent = message;
  toast.hidden = false;
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => toast.hidden = true, 3000);
}

function escapeHtml(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

async function api(path, options = {}) {
  const response = await fetch(path, {
    cache: "no-store",
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...(options.headers || {})
    }
  });
  const text = await response.text();
  let body = {};
  try { body = text ? JSON.parse(text) : {}; } catch { body = { raw: text }; }
  if (!response.ok) throw new Error(body.reason || `${response.status} ${response.statusText}`);
  return body;
}

function stateClass(value) {
  if (["normal", "closed", "ready", "disarmed", "stay", "away"].includes(value)) return "ok";
  if (["alarm", "fault", "jammed", "timeout", "tamper"].includes(value)) return "alarm";
  return "warning";
}

function armLabel(value) {
  return ({ disarmed: "ЗНЯТО", stay: "НІЧНИЙ", away: "ПІД ОХОРОНОЮ", alarm: "ТРИВОГА" })[value] || "—";
}

function wifiStateLabel(value) {
  return ({ connected: "Підключено", connecting: "Підключення…", idle: "Не налаштовано", error: "Помилка" })[value] || "Перевірка…";
}

function renderZones(data) {
  const zones = Array.isArray(data?.zones) ? data.zones : [];
  document.querySelector("#zoneCount").textContent = zones.length || "—";
  document.querySelector("#zones").innerHTML = zones.length ? zones.map(zone => `
    <div class="zone">
      <span>${escapeHtml(zone.name || `Зона ${zone.id}`)}${zone.alwaysOn ? " · 24/7" : ""}</span>
      <strong class="${stateClass(zone.state)}">${escapeHtml(zone.state)}</strong>
    </div>`).join("") : "<div class=\"zone\"><span>Дані ще не отримані</span><strong>—</strong></div>";
}

function renderPartitions(data) {
  const partition = Array.isArray(data?.partitions) ? data.partitions[0] : null;
  document.querySelector("#securityMode").textContent = armLabel(partition?.armState);
}

function renderEvents(data) {
  const events = Array.isArray(data?.events) ? data.events.slice(-6).reverse() : [];
  const list = document.querySelector("#eventList");
  list.innerHTML = events.length ? events.map(item => `
    <div><i></i><time>#${escapeHtml(item.sequence ?? "—")}</time><span>${escapeHtml(item.event || "Подія")}</span><a>${escapeHtml(item.severity || "info")}</a></div>`).join("") :
    "<div><i></i><time>—</time><span>Подій ще немає</span><a>Інформація ›</a></div>";
}

function renderOutputs(data) {
  const outputs = Array.isArray(data?.outputs) ? data.outputs : [];
  const target = document.querySelector("#ioState");
  target.innerHTML = outputs.length ? outputs.map(item => `
    <div class="${item.active ? "" : "muted"}"><b>⇆</b><span>Вих. ${Number(item.id) || 0}</span><small>${item.active ? "Увімк." : "Вимк."}</small></div>`).join("") :
    "<div><span>Очікування реальних даних контролера…</span></div>";
}

function renderNetwork(status) {
  const state = status?.state || "error";
  const ssid = status?.ssid || "—";
  const ip = status?.ip || "—";
  document.querySelector("#wifiName").textContent = ssid;
  document.querySelector("#connection").textContent = wifiStateLabel(state);
  document.querySelector("#networkState").textContent = wifiStateLabel(state);
  document.querySelector("#networkSsid").textContent = ssid;
  document.querySelector("#networkIp").textContent = ip;
  if (ssid !== "—" && !document.querySelector("#wifiSsid").value) {
    document.querySelector("#wifiSsid").value = ssid;
  }
}

async function refreshNetwork() {
  try {
    const status = await api("/api/v1/network/status");
    renderNetwork(status);
    return status;
  } catch (error) {
    renderNetwork({ state: "error", ssid: "—", ip: "—" });
    document.querySelector("#wifiResult").textContent = `Помилка: ${error.message}`;
    return null;
  }
}

async function refresh() {
  const requests = await Promise.allSettled([
    api("/api/v1/system/zones"),
    api("/api/v1/system/partitions"),
    api("/api/v1/system/outputs"),
    api("/api/v1/system/events"),
    api("/api/v1/build"),
    api("/api/v1/network/status")
  ]);

  if (requests[0].status === "fulfilled") renderZones(requests[0].value);
  if (requests[1].status === "fulfilled") renderPartitions(requests[1].value);
  if (requests[2].status === "fulfilled") renderOutputs(requests[2].value);
  if (requests[3].status === "fulfilled") renderEvents(requests[3].value);
  if (requests[4].status === "fulfilled") document.querySelector("#buildInfo").textContent = JSON.stringify(requests[4].value, null, 2);
  if (requests[5].status === "fulfilled") renderNetwork(requests[5].value);
}

async function scanWifi() {
  const button = document.querySelector("#wifiScan");
  const state = document.querySelector("#scanState");
  const list = document.querySelector("#wifiNetworks");
  button.disabled = true;
  state.textContent = "Сканування…";
  list.innerHTML = "";
  try {
    const result = await api("/api/v1/network/scan");
    const networks = Array.isArray(result.networks) ? result.networks : [];
    state.textContent = networks.length ? `Знайдено мереж: ${networks.length}` : "Мереж не знайдено";
    list.innerHTML = networks.map(item => {
      const ssid = String(item.ssid || "");
      return `<button type="button" class="wifi-network" data-ssid="${escapeHtml(ssid)}" style="display:flex;justify-content:space-between;align-items:center;width:100%;padding:11px 14px;text-align:left">
        <strong>${escapeHtml(ssid || "(прихована мережа)")}</strong><span>${Number(item.rssi) || 0} dBm</span>
      </button>`;
    }).join("");
    list.querySelectorAll("[data-ssid]").forEach(item => {
      item.onclick = () => {
        document.querySelector("#wifiSsid").value = item.dataset.ssid;
        document.querySelector("#wifiPassword").focus();
      };
    });
  } catch (error) {
    state.textContent = `Помилка: ${error.message}`;
  } finally {
    button.disabled = false;
  }
}

async function connectWifi() {
  const ssid = document.querySelector("#wifiSsid").value.trim();
  const password = document.querySelector("#wifiPassword").value;
  const button = document.querySelector("#wifiConnect");
  const result = document.querySelector("#wifiResult");
  if (!ssid) {
    result.textContent = "Помилка: виберіть мережу";
    return;
  }
  if (password.length > 0 && password.length < 8) {
    result.textContent = "Помилка: пароль має містити щонайменше 8 символів";
    return;
  }

  button.disabled = true;
  result.textContent = "Підключення…";
  document.querySelector("#networkState").textContent = "Підключення…";
  try {
    await api("/api/v1/network/connect", {
      method: "POST",
      body: JSON.stringify({ ssid, password })
    });
    document.querySelector("#wifiPassword").value = "";

    let connected = false;
    for (let attempt = 0; attempt < 10; attempt += 1) {
      await new Promise(resolve => setTimeout(resolve, 1500));
      const status = await refreshNetwork();
      if (status?.state === "connected") {
        result.textContent = `Підключено · IP ${status.ip || "—"}`;
        connected = true;
        break;
      }
    }
    if (!connected) {
      result.textContent = "Помилка: не вдалося отримати підключення. Перевірте пароль і спробуйте ще раз.";
      document.querySelector("#networkState").textContent = "Помилка";
    }
  } catch (error) {
    result.textContent = `Помилка: ${error.message}`;
    document.querySelector("#networkState").textContent = "Помилка";
  } finally {
    button.disabled = false;
  }
}

async function sendSecurityCommand(button) {
  const command = button.dataset.command;
  const actor = document.querySelector("#operatorId").value.trim();
  const credential = document.querySelector("#operatorPin").value.trim();
  if (!command) return;
  if (!actor || !/^\d{4,12}$/.test(credential)) {
    showToast("Введіть ID користувача та PIN 4–12 цифр");
    return;
  }
  const buttons = [...document.querySelectorAll("[data-command]")];
  buttons.forEach(item => item.disabled = true);
  try {
    const result = await api("/api/v1/system/security-command", {
      method: "POST",
      body: JSON.stringify({ command, actor, credential })
    });
    showToast(result.ok ? "Команду виконано" : "Команду відхилено");
    await refresh();
  } catch (error) {
    showToast(`Помилка команди: ${error.message}`);
  } finally {
    document.querySelector("#operatorPin").value = "";
    buttons.forEach(item => item.disabled = false);
  }
}

function openNetworkPage() {
  document.querySelector("#networkPage").hidden = false;
  document.querySelector("#networkPage").scrollIntoView({ behavior: "smooth", block: "start" });
  refreshNetwork();
}

function openSystemPage() {
  const system = document.querySelector("#system");
  system.hidden = false;
  system.scrollIntoView({ behavior: "smooth", block: "start" });
  refresh();
}

function bindNavigation() {
  document.querySelectorAll(".sidebar nav a").forEach(link => {
    link.addEventListener("click", event => {
      document.querySelectorAll(".sidebar nav a").forEach(item => item.classList.remove("active"));
      link.classList.add("active");
      if (link.id === "networkNav") {
        event.preventDefault();
        openNetworkPage();
      } else if (link.getAttribute("href") === "#system") {
        event.preventDefault();
        openSystemPage();
      }
    });
  });
  document.querySelector("#networkCard").addEventListener("click", openNetworkPage);
}

function bindCommandButtons() {
  document.querySelectorAll("[data-command]").forEach(button => {
    button.onclick = () => sendSecurityCommand(button);
  });
}

document.querySelector("#wifiScan").onclick = scanWifi;
document.querySelector("#wifiConnect").onclick = connectWifi;
document.querySelector("#refresh").onclick = refresh;
bindNavigation();
bindCommandButtons();
refresh();
setInterval(refresh, 5000);
