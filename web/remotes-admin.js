"use strict";

(() => {
  const API = "/api/v1/access/remotes";
  let cache = { count: 0, capacity: 8, users: [], remotes: [] };

  function esc(value) {
    return String(value ?? "").replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/\"/g, "&quot;").replace(/'/g, "&#39;");
  }

  function actor() {
    return window.HomeGuardAuth?.actor?.() || "";
  }

  function isAdmin() {
    return window.HomeGuardAuth?.role?.() === "admin" || window.HomeGuardAuth?.capabilities?.()?.accessManage === true;
  }

  async function request(options = {}) {
    const response = await fetch(API, {
      cache: "no-store",
      ...options,
      headers: { "Content-Type": "application/json", ...(options.headers || {}) },
    });
    const body = await response.json().catch(() => ({}));
    if (!response.ok || body.ok === false) throw new Error(body.reason || `${response.status}`);
    return body;
  }

  function permissionSummary(p = {}) {
    const labels = [];
    if (p.armHome) labels.push("Нічний");
    if (p.armAway) labels.push("Охорона");
    if (p.disarm) labels.push("Зняти");
    if (p.light) labels.push("Світло");
    if (p.lockPulse) labels.push("Замок");
    if (p.panic) labels.push("SOS");
    return labels.length ? labels.join(" · ") : "Без команд";
  }

  function ensurePanel() {
    if (!isAdmin()) return null;
    let panel = document.querySelector("#bleRemotesAdmin");
    if (panel) return panel;
    const system = document.querySelector("#system");
    if (!system) return null;
    panel = document.createElement("div");
    panel.id = "bleRemotesAdmin";
    panel.className = "panel";
    panel.style.marginTop = "18px";
    panel.innerHTML = `
      <div style="display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap">
        <div><h3 style="margin:0">BLE брелки</h3><small id="bleRemoteCount">0 / 8</small></div>
        <button id="bleRemoteAdd" type="button">＋ Додати брелок</button>
      </div>
      <div id="bleRemoteState" style="margin:10px 0;color:#667085">Завантаження…</div>
      <div id="bleRemoteList"></div>
      <div id="bleRemoteEditor" hidden style="margin-top:14px;padding-top:14px;border-top:1px solid #e4e7ec">
        <h4 id="bleRemoteEditorTitle">Додати брелок</h4>
        <input id="bleRemoteOriginal" type="hidden">
        <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:10px">
          <label>Назва<input id="bleRemoteName" maxlength="31" placeholder="Напр. Брелок Віктора" style="display:block;width:100%;margin-top:5px"></label>
          <label>Користувач<select id="bleRemoteOwner" style="display:block;width:100%;margin-top:5px"></select></label>
          <label style="grid-column:1/-1">BLE ID<input id="bleRemoteIdentity" maxlength="32" placeholder="32 hex символи" style="display:block;width:100%;margin-top:5px;font-family:monospace"></label>
        </div>
        <div style="display:flex;gap:12px;flex-wrap:wrap;margin:12px 0" id="bleRemotePermissions">
          <label><input type="checkbox" data-perm="armHome"> Нічний режим</label>
          <label><input type="checkbox" data-perm="armAway"> Під охорону</label>
          <label><input type="checkbox" data-perm="disarm"> Зняти</label>
          <label><input type="checkbox" data-perm="light"> Світло</label>
          <label><input type="checkbox" data-perm="lockPulse"> Замок 5 с</label>
          <label><input type="checkbox" data-perm="panic"> SOS</label>
        </div>
        <div style="display:flex;gap:8px"><button id="bleRemoteSave" type="button">Зберегти</button><button id="bleRemoteCancel" type="button">Скасувати</button></div>
      </div>`;
    system.appendChild(panel);
    panel.querySelector("#bleRemoteAdd").onclick = () => editRemote(null);
    panel.querySelector("#bleRemoteCancel").onclick = closeEditor;
    panel.querySelector("#bleRemoteSave").onclick = saveRemote;
    return panel;
  }

  function render() {
    const panel = ensurePanel();
    if (!panel) return;
    panel.querySelector("#bleRemoteCount").textContent = `${cache.count || 0} / ${cache.capacity || 8}`;
    const list = panel.querySelector("#bleRemoteList");
    const state = panel.querySelector("#bleRemoteState");
    const remotes = Array.isArray(cache.remotes) ? cache.remotes : [];
    state.textContent = remotes.length ? `У системі ${remotes.length} брелк${remotes.length === 1 ? "ок" : "и/ів"}` : "Брелків ще немає";
    list.innerHTML = remotes.map((remote, index) => `
      <div style="display:grid;grid-template-columns:minmax(140px,1.2fr) minmax(140px,1fr) minmax(180px,2fr) auto;gap:10px;align-items:center;padding:11px 0;border-top:1px solid #eef0f3">
        <div><strong>${esc(remote.name)}</strong><small style="display:block;font-family:monospace">${esc(remote.identity)}</small></div>
        <div><small>Користувач</small><strong style="display:block">${esc(remote.ownerName || remote.ownerUserId)}</strong>${remote.ownerEnabled === false ? '<small style="color:#b42318">Вимкнений</small>' : ""}</div>
        <div><small>Дозволено</small><span style="display:block">${esc(permissionSummary(remote.permissions))}</span></div>
        <div style="display:flex;gap:6px"><button type="button" data-edit="${index}">Змінити</button><button type="button" data-delete="${index}">Видалити</button></div>
      </div>`).join("");
    list.querySelectorAll("[data-edit]").forEach(button => button.onclick = () => editRemote(remotes[Number(button.dataset.edit)]));
    list.querySelectorAll("[data-delete]").forEach(button => button.onclick = () => removeRemote(remotes[Number(button.dataset.delete)]));
  }

  function fillUsers(selected = "") {
    const select = document.querySelector("#bleRemoteOwner");
    if (!select) return;
    const users = (cache.users || []).filter(user => user.enabled !== false);
    select.innerHTML = users.map(user => `<option value="${esc(user.id)}" ${user.id === selected ? "selected" : ""}>${esc(user.name || user.id)} (${esc(user.role)})</option>`).join("");
  }

  function editRemote(remote) {
    const panel = ensurePanel();
    if (!panel) return;
    const editor = panel.querySelector("#bleRemoteEditor");
    editor.hidden = false;
    panel.querySelector("#bleRemoteEditorTitle").textContent = remote ? "Змінити брелок" : "Додати брелок";
    panel.querySelector("#bleRemoteOriginal").value = remote?.identity || "";
    panel.querySelector("#bleRemoteIdentity").value = remote?.identity || "";
    panel.querySelector("#bleRemoteIdentity").readOnly = Boolean(remote);
    panel.querySelector("#bleRemoteName").value = remote?.name || "";
    fillUsers(remote?.ownerUserId || "");
    panel.querySelectorAll("[data-perm]").forEach(box => { box.checked = Boolean(remote?.permissions?.[box.dataset.perm]); });
    panel.querySelector("#bleRemoteName").focus();
  }

  function closeEditor() {
    const editor = document.querySelector("#bleRemoteEditor");
    if (editor) editor.hidden = true;
  }

  async function saveRemote() {
    const panel = ensurePanel();
    if (!panel) return;
    const name = panel.querySelector("#bleRemoteName").value.trim();
    const ownerUserId = panel.querySelector("#bleRemoteOwner").value;
    const identity = panel.querySelector("#bleRemoteIdentity").value.trim().toLowerCase();
    if (!name || !/^[0-9a-f]{32}$/.test(identity) || !ownerUserId) {
      panel.querySelector("#bleRemoteState").textContent = "Заповни назву, користувача і правильний BLE ID (32 hex символи)";
      return;
    }
    const payload = { actor: actor(), identity, ownerUserId, name };
    panel.querySelectorAll("[data-perm]").forEach(box => { payload[box.dataset.perm] = box.checked; });
    try {
      await request({ method: "POST", body: JSON.stringify(payload) });
      closeEditor();
      await refresh();
    } catch (error) {
      panel.querySelector("#bleRemoteState").textContent = `Не збережено: ${error.message}`;
    }
  }

  async function removeRemote(remote) {
    if (!remote || !confirm(`Видалити брелок «${remote.name}»?`)) return;
    try {
      await request({ method: "DELETE", body: JSON.stringify({ actor: actor(), identity: remote.identity }) });
      await refresh();
    } catch (error) {
      const state = document.querySelector("#bleRemoteState");
      if (state) state.textContent = `Не видалено: ${error.message}`;
    }
  }

  async function refresh() {
    if (!isAdmin()) return;
    const panel = ensurePanel();
    if (!panel) return;
    try {
      cache = await request();
      render();
    } catch (error) {
      panel.querySelector("#bleRemoteState").textContent = `Брелки недоступні: ${error.message}`;
    }
  }

  window.HomeGuardRemotes = { refresh };
  setInterval(refresh, 10000);
  document.addEventListener("visibilitychange", () => { if (!document.hidden) refresh(); });
  setTimeout(refresh, 1000);
})();
