#!/usr/bin/env python3
"""Headless-browser regression gate for HomeGuard-S3 navigation.

Loads the real Web UI in Chromium for every sidebar hash route and verifies the
runtime DOM, not just static source: exactly one active sidebar anchor for every
known route, the active anchor matches the URL hash, dashboard/network/system
visibility follows the route, and unknown hashes never create multiple active
items.
"""
from __future__ import annotations

import argparse
import os
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
        if self.in_nav:
            if tag == "nav":
                self.nav_depth += 1
            if tag == "a":
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


def dump_dom(chrome: str, url: str) -> str:
    command = [
        chrome,
        "--headless",
        "--no-sandbox",
        "--disable-gpu",
        "--disable-dev-shm-usage",
        "--virtual-time-budget=3500",
        "--dump-dom",
        url,
    ]
    result = subprocess.run(command, check=False, capture_output=True, text=True, timeout=20)
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

    dashboard_ids = {"overview"}
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
    if target not in parsed.ids and target not in dashboard_ids:
        errors.append(f"{route}: runtime target id missing: {target}")


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

        # Unknown hashes are allowed to select no menu item, but never more than one.
        try:
            unknown = parse_dom(dump_dom(chrome, base + "#unknown-route"))
            unknown_active = [href for href, classes in unknown.anchors if "active" in classes]
            if len(unknown_active) > 1:
                errors.append(f"unknown route produced multiple active items: {unknown_active}")
        except Exception as exc:
            errors.append(f"unknown route browser audit failed: {exc}")
    finally:
        server.shutdown()
        server.server_close()

    if errors:
        print("Web navigation runtime audit FAIL")
        for error in errors:
            print(f" - {error}")
        return 1

    print("Web navigation runtime audit PASS")
    print(f" - browser-rendered hash routes checked: {len(EXPECTED_ROUTES)}")
    print(" - exactly one active sidebar item on every known route")
    print(" - unique hrefs + target visibility verified at runtime")
    print(" - unknown hash cannot create a double-active state")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
