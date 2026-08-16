;(() => {
  const bannerId = "firstAdminBanner";
  let statusTimer = null;
  let bruceRetry = 0;

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
          <small>Створіть першого Admin перед налаштуванням Wi-Fi та інших параметрів.</small>
        </div>
        <button id="firstAdminOpen" type="button">Створити першого Admin</button>
      </div>`;
    main.insertBefore(banner, statusGrid);

    banner.querySelector("#firstAdminOpen")?.addEventListener("click", () => {
      window.location.hash = "#system";
      setTimeout(() => {
        const panel = document.getElementById("accessPanel");
        panel?.scrollIntoView({ behavior: "smooth", block: "start" });
        document.getElementById("managedUserId")?.focus();
      }, 180);
    });
  }

  async function refreshFirstAdminHint() {
    try {
      const response = await fetch(`/api/v1/access/status?ts=${Date.now()}`, {
        cache: "no-store",
        headers: { "Accept": "application/json" }
      });
      const body = await response.json().catch(() => ({}));
      if (!response.ok || body?.ok === false) throw new Error("status unavailable");

      const allowed = body?.bootstrapAllowed === true && Number(body?.userCount || 0) === 0;
      if (allowed) showBanner();
      else removeBanner();
    } catch (_) {
      // Do not turn a transient boot-time HTTP race into a permanent missing
      // bootstrap control. Keep the previous UI state and retry shortly.
    }
  }

  function scheduleStatusRefresh() {
    if (statusTimer !== null) return;
    statusTimer = window.setInterval(refreshFirstAdminHint, 1500);
  }

  function refreshBruceSource() {
    const image = document.getElementById("bruceArt");
    if (!image || image.dataset.hgBruceBound === "1") return;
    image.dataset.hgBruceBound = "1";

    const loadFresh = () => {
      image.src = `/bruce.jpg?rev=${Date.now()}`;
    };

    image.addEventListener("error", () => {
      if (bruceRetry >= 4) return;
      bruceRetry += 1;
      window.setTimeout(loadFresh, 350 * bruceRetry);
    });
    image.addEventListener("load", () => { bruceRetry = 0; });

    // The image response is deliberately no-store. A unique query also avoids
    // a browser retaining an earlier failed /bruce.jpg request across firmware
    // revisions while preserving the approved image bytes unchanged.
    loadFresh();
  }

  function start() {
    refreshBruceSource();
    refreshFirstAdminHint();
    scheduleStatusRefresh();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", start, { once: true });
  } else {
    start();
  }

  window.addEventListener("focus", () => {
    refreshBruceSource();
    refreshFirstAdminHint();
  });
})();
