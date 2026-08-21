#!/usr/bin/env python3
"""Single-browser regression gate for HomeGuard-S3 navigation.

The static contract already validates route names and DOM structure. This check
therefore keeps Chromium focused on the behavior only: one live document, real
clicks/hashchange events, visibility changes, focus safety, burst routing, and
same-hash clicks. Avoid spawning a fresh Chrome process for every hash route;
that made CI slower and flaky without adding useful coverage.
"""
from __future__ import annotations

import argparse
import html
import json
import os
import re
import shutil
import subprocess
import threading
import time
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / "web"
EXPECTED_ROUTES = (
    "#overview",
    "#zones-section",
    "#zones",
    "#io-section",
    "#ioState",
    "#events",
    "#history",
    "#networkPage",
    "#system",
)


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, fmt: str, *args: object) -> None:
        return


def find_chrome() -> str:
    candidates = [
        os.environ.get("CHROME_BIN", ""),
        "google-chrome",
        "google-chrome-stable",
        "chromium",
        "chromium-browser",
    ]
    for candidate in candidates:
        if candidate and shutil.which(candidate):
            return shutil.which(candidate) or candidate
    raise RuntimeError("Chrome/Chromium executable not found")


def dump_dom(chrome: str, url: str, budget_ms: int = 7000) -> str:
    command = [
        chrome,
        "--headless",
        "--no-sandbox",
        "--disable-gpu",
        "--disable-dev-shm-usage",
        "--disable-background-networking",
        "--disable-component-update",
        "--disable-default-apps",
        "--no-first-run",
        f"--virtual-time-budget={budget_ms}",
        "--dump-dom",
        url,
    ]
    result = subprocess.run(command, check=False, capture_output=True, text=True, timeout=20)
    if result.returncode != 0:
        raise RuntimeError(f"Chromium failed for {url}: {result.stderr[-1200:]}")
    return result.stdout


def sequence_probe_html(index_html: str) -> str:
    routes_json = json.dumps(EXPECTED_ROUTES)
    probe = f"""
<script id="homeguard-nav-sequence-probe">
(async () => {{
  const routes = {routes_json};
  const failures = [];
  const wait = (ms = 55) => new Promise(resolve => setTimeout(resolve, ms));
  const activeHrefs = () => [...document.querySelectorAll('.sidebar nav a.active')].map(a => a.getAttribute('href'));
  const hidden = id => {{
    const el = document.getElementById(id);
    return !el || el.hidden || getComputedStyle(el).display === 'none';
  }};
  const check = (route, phase) => {{
    const active = activeHrefs();
    if (active.length !== 1 || active[0] !== route) failures.push(`${{phase}}:${{route}} active=${{JSON.stringify(active)}}`);

    const group = document.querySelector('.nav-group-label');
    if (!group || group.matches('a') || group.classList.contains('active')) failures.push(`${{phase}}:${{route}} settings-group-active`);

    if (route === '#networkPage') {{
      if (hidden('networkPage')) failures.push(`${{phase}}:${{route}} network-hidden`);
      if (!hidden('system')) failures.push(`${{phase}}:${{route}} system-visible`);
    }} else if (route === '#system') {{
      if (hidden('system')) failures.push(`${{phase}}:${{route}} system-hidden`);
      if (!hidden('networkPage')) failures.push(`${{phase}}:${{route}} network-visible`);
    }} else {{
      if (!hidden('networkPage') || !hidden('system')) failures.push(`${{phase}}:${{route}} settings-page-leak`);
    }}
  }};

  await wait(100);
  check(window.location.hash || '#overview', 'cold');

  // Real user path: click each sidebar item in one live document.
  for (const route of routes) {{
    const link = document.querySelector(`.sidebar nav a[href="${{route}}"]`);
    if (!link) {{ failures.push(`click:${{route}} missing-link`); continue; }}
    link.click();
    await wait();
    check(route, 'click');
  }}

  // Focus must never steal the authoritative active state.
  const current = window.location.hash || '#overview';
  for (const link of document.querySelectorAll('.sidebar nav a')) {{
    link.focus();
    await wait(5);
    check(current, 'focus');
  }}

  // Unknown hashes may select no item, but may never create double-active.
  window.location.hash = '#unknown-route';
  await wait();
  const unknownActive = activeHrefs();
  if (unknownActive.length > 1) failures.push(`unknown active=${{JSON.stringify(unknownActive)}}`);

  // Burst routing catches competing/stale hashchange controllers.
  window.location.hash = '#zones-section';
  window.location.hash = '#ioState';
  window.location.hash = '#networkPage';
  window.location.hash = '#system';
  await wait(140);
  check('#system', 'burst');

  // Same-hash click exercises the explicit same-route branch.
  const systemLink = document.querySelector('.sidebar nav a[href="#system"]');
  if (!systemLink) {{
    failures.push('same-hash-click:#system missing-link');
  }} else {{
    systemLink.click();
    await wait();
    check('#system', 'same-hash-click');
  }}

  const marker = document.createElement('div');
  marker.id = 'homeguard-nav-sequence-result';
  marker.hidden = true;
  marker.dataset.result = failures.length ? 'FAIL' : 'PASS';
  marker.dataset.failures = failures.join('|');
  document.body.appendChild(marker);
}})();
</script>
"""
    if "</body>" not in index_html:
        raise RuntimeError("index.html has no </body> for sequence probe injection")
    return index_html.replace("</body>", probe + "\n</body>", 1)


def run_sequence_probe(chrome: str, web_root: Path, base: str) -> list[str]:
    probe_name = "__homeguard_navigation_sequence_probe.html"
    probe_path = web_root / probe_name
    errors: list[str] = []
    try:
        index_html = (web_root / "index.html").read_text(encoding="utf-8")
        probe_path.write_text(sequence_probe_html(index_html), encoding="utf-8")
        dom = dump_dom(chrome, base + probe_name + "#overview")
        marker = re.search(
            r'id="homeguard-nav-sequence-result"[^>]*data-result="([^"]+)"[^>]*data-failures="([^"]*)"',
            dom,
        )
        if not marker:
            errors.append("browser transition probe did not complete")
        elif marker.group(1) != "PASS":
            errors.extend(filter(None, html.unescape(marker.group(2)).split("|")))
    except Exception as exc:
        errors.append(f"browser audit failed: {exc}")
    finally:
        try:
            probe_path.unlink()
        except FileNotFoundError:
            pass
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--web", default=str(WEB))
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8771)
    args = parser.parse_args()

    web_root = Path(args.web).resolve()
    for asset in ("index.html", "app.css", "app.js", "access-session.js"):
        if not (web_root / asset).is_file():
            print(f"FAIL: missing web asset {web_root / asset}")
            return 1

    chrome = find_chrome()
    handler = lambda *a, **kw: QuietHandler(*a, directory=str(web_root), **kw)
    server = ThreadingHTTPServer((args.host, args.port), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    time.sleep(0.2)

    try:
        base = f"http://{args.host}:{args.port}/"
        errors = run_sequence_probe(chrome, web_root, base)
    finally:
        server.shutdown()
        server.server_close()

    if errors:
        print("Web navigation runtime audit FAIL")
        for error in errors:
            print(f" - {error}")
        return 1

    print("Web navigation runtime audit PASS")
    print(f" - real click/hash routes checked in one browser document: {len(EXPECTED_ROUTES)}")
    print(" - settings page visibility checked after every route")
    print(" - focus cannot mutate the single active navigation owner")
    print(" - unknown hash cannot create a double-active state")
    print(" - rapid hash burst and same-hash click are race-safe")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
