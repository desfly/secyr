#!/usr/bin/env python3
"""Headless-browser regression gate for HomeGuard-S3 navigation.

Checks both cold loads and in-document transitions. The latter is important for
this UI because click handlers, hashchange listeners and the firmware-served JS
suffix all react to the same route change. The invariant is simple: at most one
sidebar anchor may own the blue ``active`` state, and every known route must end
with exactly its own anchor active.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import threading
import time
from html.parser import HTMLParser
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


class NavParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.in_nav = False
        self.nav_depth = 0
        self.anchors: list[tuple[str, set[str]]] = []
        self.hidden_ids: set[str] = set()
        self.ids: set[str] = set()

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        values = dict(attrs)
        if tag == "nav":
            self.in_nav = True
            self.nav_depth = 1
            return
        if self.in_nav and tag == "a":
            href = values.get("href") or ""
            classes = set((values.get("class") or "").split())
            self.anchors.append((href, classes))
        element_id = values.get("id")
        if element_id:
            self.ids.add(element_id)
            if "hidden" in values:
                self.hidden_ids.add(element_id)

    def handle_endtag(self, tag: str) -> None:
        if self.in_nav and tag == "nav":
            self.nav_depth -= 1
            if self.nav_depth <= 0:
                self.in_nav = False


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


def dump_dom(chrome: str, url: str, budget_ms: int = 3500) -> str:
    command = [
        chrome,
        "--headless",
        "--no-sandbox",
        "--disable-gpu",
        "--disable-dev-shm-usage",
        f"--virtual-time-budget={budget_ms}",
        "--dump-dom",
        url,
    ]
    result = subprocess.run(command, check=False, capture_output=True, text=True, timeout=25)
    if result.returncode != 0:
        raise RuntimeError(f"Chromium failed for {url}: {result.stderr[-1200:]}")
    return result.stdout


def parse_dom(dom: str) -> NavParser:
    parser = NavParser()
    parser.feed(dom)
    return parser


def assert_route(route: str, parsed: NavParser, errors: list[str]) -> None:
    routes = [href for href, _ in parsed.anchors if href.startswith("#")]
    active = [href for href, classes in parsed.anchors if "active" in classes]

    if tuple(routes) != EXPECTED_ROUTES:
        errors.append(f"{route}: runtime nav routes changed/reordered: {routes}")
    if len(routes) != len(set(routes)):
        errors.append(f"{route}: duplicate runtime hrefs: {routes}")
    if active != [route]:
        errors.append(f"{route}: expected exactly one active href {route}, got {active}")

    if route == "#networkPage":
        if "networkPage" in parsed.hidden_ids:
            errors.append(f"{route}: networkPage remained hidden")
        if "system" not in parsed.hidden_ids:
            errors.append(f"{route}: system should be hidden")
    elif route == "#system":
        if "system" in parsed.hidden_ids:
            errors.append(f"{route}: system remained hidden")
        if "networkPage" not in parsed.hidden_ids:
            errors.append(f"{route}: networkPage should be hidden")
    else:
        if "networkPage" not in parsed.hidden_ids or "system" not in parsed.hidden_ids:
            errors.append(f"{route}: dashboard route leaked a hidden page")

    target = route[1:]
    if target not in parsed.ids and target != "overview":
        errors.append(f"{route}: runtime target id missing: {target}")


def sequence_probe_html(index_html: str) -> str:
    routes_json = json.dumps(EXPECTED_ROUTES)
    probe = f"""
<script id="homeguard-nav-sequence-probe">
(async () => {{
  const routes = {routes_json};
  const failures = [];
  const wait = (ms = 45) => new Promise(resolve => setTimeout(resolve, ms));
  const activeHrefs = () => [...document.querySelectorAll('.sidebar nav a.active')].map(a => a.getAttribute('href'));
  const check = (route, phase) => {{
    const active = activeHrefs();
    if (active.length !== 1 || active[0] !== route) failures.push(`${{phase}}:${{route}} active=${{JSON.stringify(active)}}`);
    const group = document.querySelector('.nav-group-label');
    if (!group || group.matches('a') || group.classList.contains('active')) failures.push(`${{phase}}:${{route}} settings-group-active`);

    const network = document.getElementById('networkPage');
    const system = document.getElementById('system');
    const dashboard = [...document.querySelectorAll('.status-grid,.two-col')];
    const isNetwork = route === '#networkPage';
    const isSystem = route === '#system';
    if (!network || network.hidden === isNetwork) failures.push(`${{phase}}:${{route}} network-visibility`);
    if (!system || system.hidden === isSystem) failures.push(`${{phase}}:${{route}} system-visibility`);
    if (dashboard.some(node => node.hidden !== (isNetwork || isSystem))) failures.push(`${{phase}}:${{route}} dashboard-visibility`);
  }};

  await wait(80);
  // Real click path: bindNavigation() prevents default and changes location.hash.
  for (const route of routes) {{
    const link = document.querySelector(`.sidebar nav a[href="${{route}}"]`);
    if (!link) {{ failures.push(`click:${{route}} missing-link`); continue; }}
    link.click();
    await wait();
    check(route, 'click');
  }}

  // Focus must never mutate the authoritative active class.
  const current = window.location.hash || '#overview';
  for (const link of document.querySelectorAll('.sidebar nav a')) {{
    link.focus();
    await wait(5);
    check(current, 'focus');
  }}

  // Burst routing catches stale callbacks / competing hashchange controllers.
  window.location.hash = '#zones-section';
  window.location.hash = '#ioState';
  window.location.hash = '#networkPage';
  window.location.hash = '#system';
  await wait(120);
  check('#system', 'burst');

  // Same-hash click takes the explicit routeFromHash() branch in bindNavigation.
  const systemLink = document.querySelector('.sidebar nav a[href="#system"]');
  if (systemLink) {{ systemLink.click(); await wait(); check('#system', 'same-hash-click'); }}

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


def run_sequence_probe(chrome: str, web_root: Path, base: str, errors: list[str]) -> None:
    probe_name = "__homeguard_navigation_sequence_probe.html"
    probe_path = web_root / probe_name
    try:
        probe_path.write_text(sequence_probe_html((web_root / "index.html").read_text(encoding="utf-8")), encoding="utf-8")
        dom = dump_dom(chrome, base + probe_name + "#overview", budget_ms=6500)
        marker = re.search(
            r'id="homeguard-nav-sequence-result"[^>]*data-result="([^"]+)"[^>]*data-failures="([^"]*)"',
            dom,
        )
        if not marker:
            errors.append("transition probe did not complete in Chromium")
        elif marker.group(1) != "PASS":
            errors.append(f"transition probe failed: {marker.group(2)}")
    except Exception as exc:
        errors.append(f"transition probe browser audit failed: {exc}")
    finally:
        try:
            probe_path.unlink()
        except FileNotFoundError:
            pass


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

    errors: list[str] = []
    try:
        base = f"http://{args.host}:{args.port}/"
        for route in EXPECTED_ROUTES:
            try:
                assert_route(route, parse_dom(dump_dom(chrome, base + route)), errors)
            except Exception as exc:
                errors.append(f"{route}: browser audit failed: {exc}")

        # Unknown hashes may select no item, but can never create a double-active state.
        try:
            unknown = parse_dom(dump_dom(chrome, base + "#unknown-route"))
            unknown_active = [href for href, classes in unknown.anchors if "active" in classes]
            if len(unknown_active) > 1:
                errors.append(f"unknown route produced multiple active items: {unknown_active}")
        except Exception as exc:
            errors.append(f"unknown route browser audit failed: {exc}")

        run_sequence_probe(chrome, web_root, base, errors)
    finally:
        server.shutdown()
        server.server_close()

    if errors:
        print("Web navigation runtime audit FAIL")
        for error in errors:
            print(f" - {error}")
        return 1

    print("Web navigation runtime audit PASS")
    print(f" - browser-rendered cold-load hash routes checked: {len(EXPECTED_ROUTES)}")
    print(" - click + hashchange transitions checked in one live document")
    print(" - rapid hash burst and same-hash click are race-safe")
    print(" - route visibility stays coherent during live transitions")
    print(" - focus cannot mutate the single active navigation owner")
    print(" - unique hrefs + target visibility verified at runtime")
    print(" - unknown hash cannot create a double-active state")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
