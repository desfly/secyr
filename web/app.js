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
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...(options.headers || {})
    }
  });
  if (!response.ok) throw new Error(`${response.status} ${response.statusText}`);
  return response.json();
}

function stateClass(value) {
  if (["normal", "closed", "open", "ready"].includes(value)) return "ok";
  if (["alarm", "fault", "jammed", "timeout", "open_circuit", "short_circuit"].includes(value)) return "alarm";
  return "warning";
}

function render(state) {
  document.querySelector("#connection").textContent =
    `Оновлено · sequence ${state.sequence}`;
  document.querySelector("#securityMode").textContent = state.security_mode;
  document.querySelector("#coldPressure").textContent =
    `${state.cold_pressure_bar.toFixed(2)} бар`;
  document.querySelector("#hotPressure").textContent =
    `${state.hot_pressure_bar.toFixed(2)} бар`;
  document.querySelector("#coldTemperature").textContent =
    `${state.cold_temperature_c.toFixed(1)} °C`;
  document.querySelector("#hotTemperature").textContent =
    `${state.hot_temperature_c.toFixed(1)} °C`;
  document.querySelector("#mains").textContent =
    state.mains_present ? "Є" : "Немає";
  document.querySelector("#battery").textContent =
    `${state.battery_voltage_v.toFixed(2)} V / ${state.battery_current_a.toFixed(2)} A`;
  document.querySelector("#light").textContent =
    state.corridor_light ? "Увімкнено" : "Вимкнено";

  document.querySelector("#zones").innerHTML = state.zones.map(zone => `
    <div class="zone">
      <span>${zone.title}${zone.always_on ? " · 24/7" : ""}</span>
      <strong class="${stateClass(zone.state)}">${zone.state}</strong>
    </div>
  `).join("");

  document.querySelector("#valves").innerHTML = state.valves.map(valve => `
    <div class="valve">
      <span>${valve.id}</span>
      <strong class="${stateClass(valve.state)}">${valve.state}</strong>
    </div>
    <div class="actions">
      <button data-command="valve.close" data-target="${valve.id}">Закрити</button>
      <button data-command="valve.open" data-target="${valve.id}" class="secondary">Відкрити</button>
    </div>
  `).join("");

  bindCommandButtons();
}

async function refresh() {
  try {
    const [state, build] = await Promise.all([
      api("/api/v1/state"),
      api("/api/v1/build")
    ]);
    render(state);
    document.querySelector("#buildInfo").textContent =
      JSON.stringify(build, null, 2);
  } catch (error) {
    document.querySelector("#connection").textContent = `Помилка: ${error.message}`;
  }
}

async function sendCommand(button) {
  button.disabled = true;
  try {
    const payload = {
      request_id: crypto.randomUUID(),
      actor,
      command: button.dataset.command,
      target: button.dataset.target || "",
      value: button.dataset.value || ""
    };
    const response = await api("/api/v1/command", {
      method: "POST",
      body: JSON.stringify(payload)
    });
    showToast(`${response.code}: ${response.message}`);
    await refresh();
  } catch (error) {
    showToast(`Помилка: ${error.message}`);
  } finally {
    button.disabled = false;
  }
}

function bindCommandButtons() {
  document.querySelectorAll("[data-command]").forEach(button => {
    button.onclick = () => sendCommand(button);
  });
}

document.querySelector("#refresh").onclick = refresh;
bindCommandButtons();
refresh();
setInterval(refresh, 5000);
