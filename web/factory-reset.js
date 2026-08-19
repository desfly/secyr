"use strict";

(() => {
  const systemPage = document.querySelector("#system");
  if (!systemPage || document.querySelector("#factoryResetPanel")) return;

  const panel = document.createElement("article");
  panel.id = "factoryResetPanel";
  panel.className = "panel";
  panel.style.cssText = "max-width:920px;margin-top:18px;border-color:#efb4b8";
  panel.innerHTML = `
    <h3>Скидання до заводських налаштувань</h3>
    <p style="margin:4px 0 12px">Стираються користувачі, Wi-Fi, Cloud/MQTT, налаштування контролера та provisioning. Апаратна ідентичність контролера зберігається.</p>
    <p style="margin:4px 0 14px"><strong>Після скидання HomeGuard перезавантажиться і зникне з поточної мережі.</strong></p>
    <div style="display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:10px;margin-bottom:10px">
      <label>Admin ID<input id="factoryResetActor" type="text" maxlength="23" autocomplete="username" placeholder="ID адміністратора" style="display:block;width:100%;margin-top:6px;padding:10px;border:1px solid #d7deea;border-radius:8px"></label>
      <label>Admin PIN<input id="factoryResetCredential" type="password" inputmode="numeric" minlength="4" maxlength="12" autocomplete="current-password" placeholder="4–12 цифр" style="display:block;width:100%;margin-top:6px;padding:10px;border:1px solid #d7deea;border-radius:8px"></label>
    </div>
    <label>Для підтвердження введіть <strong>ERASE_ALL</strong>
      <input id="factoryResetConfirm" type="text" maxlength="9" autocomplete="off" placeholder="ERASE_ALL" style="display:block;width:100%;margin-top:6px;padding:10px;border:1px solid #d7deea;border-radius:8px">
    </label>
    <div style="display:flex;gap:12px;align-items:center;flex-wrap:wrap;margin-top:14px">
      <button id="factoryResetButton" type="button" disabled style="border-color:#d72632;color:#b01924">Скинути HomeGuard</button>
      <span id="factoryResetResult">—</span>
    </div>`;
  systemPage.appendChild(panel);

  const actor = panel.querySelector("#factoryResetActor");
  const credential = panel.querySelector("#factoryResetCredential");
  const confirmation = panel.querySelector("#factoryResetConfirm");
  const button = panel.querySelector("#factoryResetButton");
  const result = panel.querySelector("#factoryResetResult");

  const valid = () => {
    button.disabled = !actor.value.trim() || !/^\d{4,12}$/.test(credential.value) || confirmation.value.trim() !== "ERASE_ALL";
  };
  [actor, credential, confirmation].forEach(field => field.addEventListener("input", valid));

  button.addEventListener("click", async () => {
    if (button.disabled) return;
    const finalConfirm = window.confirm("ОСТАТОЧНЕ ПІДТВЕРДЖЕННЯ: стерти користувачів, Wi-Fi, Cloud і всі користувацькі налаштування HomeGuard та перезавантажити контролер?");
    if (!finalConfirm) return;

    button.disabled = true;
    result.textContent = "Скидання…";
    try {
      const response = await fetch("/api/v1/system/factory-reset", {
        method: "POST",
        cache: "no-store",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          actor: actor.value.trim(),
          credential: credential.value,
          confirm: "ERASE_ALL"
        })
      });
      const text = await response.text();
      let body = {};
      try { body = text ? JSON.parse(text) : {}; } catch (_) { body = {}; }

      credential.value = "";
      confirmation.value = "";

      if (body.rebooting === true && (!response.ok || body.ok === false)) {
        result.textContent = "Скидання виконано частково. HomeGuard перезавантажується для безпечного відновлення; після запуску перевірте стан і повторіть повне скидання за потреби.";
        if (typeof showToast === "function") showToast("Factory Reset частковий · HomeGuard відновлюється");
        return;
      }

      if (!response.ok || body.ok === false) throw new Error(body.reason || `${response.status} ${response.statusText}`);

      result.textContent = "Скидання виконано. HomeGuard перезавантажується; поточне з’єднання буде втрачено.";
      if (typeof showToast === "function") showToast("Factory Reset виконано · HomeGuard перезавантажується");
    } catch (error) {
      credential.value = "";
      confirmation.value = "";
      result.textContent = `Скидання відхилено: ${error.message}`;
    } finally {
      valid();
    }
  });

  valid();
})();
