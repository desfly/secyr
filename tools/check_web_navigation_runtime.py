#!/usr/bin/env python3
"""Single-browser regression gate for HomeGuard-S3 navigation.

The static contract already validates route names and DOM structure. This check
therefore keeps Chromium focused on the behavior only: one live document, real
clicks/hashchange events, visibility changes, focus safety, burst routing, and
same-hash clicks. The browser reports the result back to the local test server;
Python then terminates Chromium explicitly instead of waiting for --dump-dom to
exit, which avoids runner-dependent hangs from long-lived page timers/network.
"""
from __future__ import annotations

import argparse
import json
import os
import queue
import shutil
import subprocess
import tempfile
import threading
import time
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / "web"
RESULT_PATH = "/__homeguard_navigation_sequence_result"
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
    result_queue: queue.Queue[dict[str, object]] | None = None

    def log_message(self, fmt: str, *args: object) -> None:
        return

    def do_POST(self) -> None:
        if self.path != RESULT_PATH:
            self.send_error(404)
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
        except (ValueError, UnicodeDecodeError, json.JSONDecodeError):
            self.send_error(400, "invalid probe result")
            return

        if not isinstance(payload, dict) or self.result_queue is None:
            self.send_error(503, "probe result queue unavailable")
            return

        try:
            self.result_queue.put_nowait(payload)
        except queue.Full:
            self.send_error(409, "probe result already received")
            return

        self.send_response(204)
        self.end_headers()


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


def run_browser_probe(
    chrome: str,
    url: str,
    result_queue: queue.Queue[dict[str, object]],
    timeout_seconds: float = 20.0,
) -> dict[str, object]:
    with tempfile.TemporaryDirectory(prefix="homeguard-nav-chrome-") as profile_dir:
        command = [
            chrome,
            "--headless",
            "--no-sandbox",
            "--disable-gpu",
            "--disable-dev-shm-usage",
            "--disable-background-networking",
            "--disable-component-update",
            "--disable-default-apps",
            "--disable-background-timer-throttling",
            "--disable-renderer-backgrounding",
            "--no-first-run",
            f"--user-data-dir={profile_dir}",
            url,
        ]
        process = subprocess.Popen(
            command,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        try:
            deadline = time.monotonic() + timeout_seconds
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError(
                        f"Chromium exited with code {process.returncode} before reporting the probe result"
                    )
                try:
                    return result_queue.get(timeout=0.2)
                except queue.Empty:
                    continue
            raise RuntimeError(
                f"browser probe callback timed out after {timeout_seconds:.0f} seconds"
            )
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=3)


def sequence_probe_html(index_html: str) -> str:
    routes_json = json.dumps(EXPECTED_ROUTES)
    result_path_json = json.dumps(RESULT_PATH)
    probe = f"""
<script id="homeguard-nav-sequence-probe">
(async () => {{
  const routes = {routes_json};
  const resultPath = {result_path_json};
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

  const result = failures.length ? 'FAIL' : 'PASS';
  const marker = document.createElement('div');
  marker.id = 'homeguard-nav-sequence-result';
  marker.hidden = true;
  marker.dataset.result = result;
  marker.dataset.failures = failures.join('|');
  document.body.appendChild(marker);

  await fetch(resultPath, {{
    method: 'POST',
    headers: {{'Content-Type': 'application/json'}},
    body: JSON.stringify({{result, failures}}),
    keepalive: true,
  }});
}})();
</script>
"""
    if "</body>" not in index_html:
        raise RuntimeError("index.html has no </body> for sequence probe injection")
    return index_html.replace("</body>", probe + "\n</body>", 1)


def run_sequence_probe(
    chrome: str,
    web_root: Path,
    base: str,
    result_queue: queue.Queue[dict[str, object]],
) -> list[str]:
    probe_name = "__homeguard_navigation_sequence_probe.html"
    probe_path = web_root / probe_name
    errors: list[str] = []
    try:
        index_html = (web_root / "index.html").read_text(encoding="utf-8")
        probe_path.write_text(sequence_probe_html(index_html), encoding="utf-8")
        payload = run_browser_probe(chrome, base + probe_name + "#overview", result_queue)

        result = payload.get("result")
        failures = payload.get("failures")
        if result not in {"PASS", "FAIL"} or not isinstance(failures, list):
            errors.append(f"browser transition probe returned malformed result: {payload!r}")
        elif result != "PASS":
            errors.extend(str(failure) for failure in failures if failure)
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
    result_queue: queue.Queue[dict[str, object]] = queue.Queue(maxsize=1)
    QuietHandler.result_queue = result_queue
    handler = lambda *a, **kw: QuietHandler(*a, directory=str(web_root), **kw)
    server = ThreadingHTTPServer((args.host, args.port), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    time.sleep(0.2)

    try:
        base = f"http://{args.host}:{args.port}/"
        errors = run_sequence_probe(chrome, web_root, base, result_queue)
    finally:
        QuietHandler.result_queue = None
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
    print(" - browser reports completion by local callback; no dump-dom exit dependency")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
