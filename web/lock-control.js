"use strict";

(() => {
  const quick = document.querySelector(".quick");
  if (!quick || document.getElementById("lockRelayButton")) return;

  const button = document.createElement("button");
  button.id = "lockRelayButton";
  button.type = "button";
  button.innerHTML = "<b>🔒</b><strong>Замок</strong><small>Відкрити на 5 секунд</small>";
  quick.appendChild(button);

  const toast = (message) => {
    if (typeof window.showToast === "function") {
      window.showToast(message);
      return;
    }
    const node = document.getElementById("toast");
    if (!node) return;
    node.textContent = message;
    node.hidden = false;
    window.setTimeout(() => { node.hidden = true; }, 2500);
  };

  button.addEventListener("click", async () => {
    if (button.disabled) return;
    const actor = document.querySelector("#operatorId")?.value?.trim() || "";
    if (!actor) {
      toast("Спочатку увійдіть у контролер");
      return;
    }

    button.disabled = true;
    try {
      const response = await fetch("/api/v1/outputs/lock/pulse", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify({actor}),
      });
      const text = await response.text();
      let body = {};
      try { body = text ? JSON.parse(text) : {}; } catch (_) {}
      if (!response.ok || body.ok === false) throw new Error(body.reason || `HTTP ${response.status}`);

      toast("Замок відкрито на 5 секунд");
      let remaining = 5;
      const strong = button.querySelector("strong");
      const small = button.querySelector("small");
      strong.textContent = "Замок відкрито";
      small.textContent = `${remaining} с`;
      const timer = window.setInterval(() => {
        remaining -= 1;
        if (remaining > 0) {
          small.textContent = `${remaining} с`;
          return;
        }
        window.clearInterval(timer);
        strong.textContent = "Замок";
        small.textContent = "Відкрити на 5 секунд";
        button.disabled = false;
      }, 1000);
    } catch (error) {
      toast(`Замок: ${error?.message || "помилка"}`);
      button.disabled = false;
    }
  });
})();
