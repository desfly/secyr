;(() => {
  const bannerId = "firstAdminBanner";

  function removeBanner() {
    document.getElementById(bannerId)?.remove();
  }

  function showBanner() {
    if (document.getElementById(bannerId)) return;
    const statusGrid = document.querySelector(".status-grid");
    const main = document.querySelector("main#overview");
    if (!statusGrid || !main) return;

    const banner = document.createElement("article");
    banner.id = bannerId;
    banner.className = "panel";
    banner.style.cssText = "margin:0 0 14px;border:2px solid #e6a700;background:#fff9df";
    banner.innerHTML = `
      <div style="display:flex;gap:14px;align-items:center;justify-content:space-between;flex-wrap:wrap">
        <div>
          <strong style="display:block;font-size:18px;margin-bottom:4px">Перший запуск після заводського скидання</strong>
          <small>Створіть першого Admin перед налаштуванням Wi‑Fi та інших параметрів.</small>
        </div>
        <button id="firstAdminOpen" type="button">Створити першого Admin</button>
      </div>`;
    main.insertBefore(banner, statusGrid);

    banner.querySelector("#firstAdminOpen")?.addEventListener("click", () => {
      window.location.hash = "#system";
      setTimeout(() => document.getElementById("accessPanel")?.scrollIntoView({ behavior: "smooth", block: "start" }), 120);
    });
  }

  async function refreshFirstAdminHint() {
    try {
      const response = await fetch("/api/v1/access/status", { cache: "no-store" });
      const body = await response.json().catch(() => ({}));
      const allowed = response.ok && body?.bootstrapAllowed === true && Number(body?.userCount || 0) === 0;
      if (allowed) showBanner();
      else removeBanner();
    } catch (_) {
      removeBanner();
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", refreshFirstAdminHint, { once: true });
  } else {
    refreshFirstAdminHint();
  }
  window.addEventListener("focus", refreshFirstAdminHint);
  setTimeout(refreshFirstAdminHint, 1200);
})();
