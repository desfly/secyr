#!/usr/bin/env python3
"""Headless-browser control smoke server for the HomeGuard-S3 Web UI.

`serve` exposes the real checked-in web assets plus deterministic mock API
responses and injects a test-only script that clicks every high-priority
control. `verify` then checks the captured HTTP requests, proving that visible
buttons actually produce the expected firmware API payloads.
"""
from __future__ import annotations

import argparse
import json
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

HARNESS = r"""
<script>
(async () => {
  const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
  const waitFor = async (selector, timeout = 4000) => {
    const deadline = Date.now() + timeout;
    while (Date.now() < deadline) {
      const item = document.querySelector(selector);
      if (item) return item;
      await sleep(50);
    }
    throw new Error(`timeout waiting for ${selector}`);
  };
  const waitEnabled = async (selector, timeout = 4000) => {
    const deadline = Date.now() + timeout;
    while (Date.now() < deadline) {
      const item = document.querySelector(selector);
      if (item && !item.disabled) return item;
      await sleep(50);
    }
    throw new Error(`timeout waiting for enabled ${selector}`);
  };
  const operator = () => {
    document.querySelector('#operatorId').value = 'smoke-user';
    document.querySelector('#operatorPin').value = '1234';
  };

  await waitFor('html[data-homeguard-ui="ready"]');
  await waitFor('[data-output-id="2"][data-output-active="true"]');

  for (const command of [
    'security.arm_away',
    'security.disarm',
    'security.arm_home',
    'security.panic'
  ]) {
    operator();
    (await waitEnabled(`[data-command="${command}"]`)).click();
    await sleep(450);
  }

  operator();
  (await waitEnabled('[data-output-id="2"][data-output-active="true"]')).click();
  await sleep(650);
  operator();
  (await waitEnabled('[data-output-id="2"][data-output-active="false"]')).click();
  await sleep(650);

  (await waitEnabled('#wifiScan')).click();
  await sleep(500);
  document.querySelector('#networkActor').value = 'admin-smoke';
  document.querySelector('#networkCredential').value = '4321';
  document.querySelector('#wifiSsid').value = 'SmokeNet';
  document.querySelector('#wifiPassword').value = 'password123';
  (await waitEnabled('#wifiConnect')).click();
  await sleep(2100);

  document.querySelector('#managedUserId').value = 'admin-smoke';
  document.querySelector('#managedUserName').value = 'Smoke Admin';
  document.querySelector('#managedUserRole').value = 'admin';
  document.querySelector('#managedUserPin').value = '4321';
  (await waitEnabled('#accessBootstrap')).click();
  await sleep(500);

  document.querySelector('#accessActor').value = 'admin-smoke';
  document.querySelector('#accessCredential').value = '4321';
  (await waitEnabled('#accessLoad')).click();
  await sleep(500);

  document.querySelector('#accessActor').value = 'admin-smoke';
  document.querySelector('#accessCredential').value = '4321';
  document.querySelector('#managedUserId').value = 'user-smoke';
  document.querySelector('#managedUserName').value = 'Smoke User';
  document.querySelector('#managedUserRole').value = 'user';
  document.querySelector('#managedUserPin').value = '5678';
  document.querySelector('#managedUserEnabled').checked = true;
  (await waitEnabled('#accessSave')).click();
  await sleep(600);

  document.documentElement.dataset.homeguardControlSmoke = 'done';
})().catch(error => {
  document.documentElement.dataset.homeguardControlSmoke = 'failed';
  document.documentElement.dataset.homeguardControlError = String(error && error.message || error);
});
</script>
"""


class SmokeState:
    def __init__(self, log_path: Path) -> None:
        self.log_path = log_path
        self.lock = threading.Lock()
        self.valve_active = False
        self.network_ssid = "InitialNet"
        self.users: list[dict[str, object]] = []
        self.sequence = 1
        self.log_path.write_text("", encoding="utf-8")

    def log(self, method: str, path: str, body: object | None = None) -> None:
        record = {"method": method, "path": path}
        if body is not None:
            record["body"] = body
        with self.lock:
            with self.log_path.open("a", encoding="utf-8") as handle:
                handle.write(json.dumps(record, ensure_ascii=False, sort_keys=True) + "\n")


class SmokeHandler(BaseHTTPRequestHandler):
    server_version = "HomeGuardWebSmoke/1.0"

    @property
    def state(self) -> SmokeState:
        return self.server.state  # type: ignore[attr-defined]

    @property
    def web_root(self) -> Path:
        return self.server.web_root  # type: ignore[attr-defined]

    def log_message(self, fmt: str, *args: object) -> None:
        return

    def _send(self, status: int, content_type: str, payload: bytes) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def _json(self, body: object, status: int = 200) -> None:
        payload = json.dumps(body, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self._send(status, "application/json; charset=utf-8", payload)

    def _asset(self, filename: str, content_type: str) -> None:
        path = self.web_root / filename
        if not path.is_file():
            self._json({"ok": False, "reason": "asset_not_found"}, 404)
            return
        self._send(200, content_type, path.read_bytes())

    def do_GET(self) -> None:  # noqa: N802
        path = urlparse(self.path).path
        self.state.log("GET", path)

        if path in ("/", "/index.html"):
            html = (self.web_root / "index.html").read_text(encoding="utf-8")
            html = html.replace("</body>", HARNESS + "\n</body>")
            self._send(200, "text/html; charset=utf-8", html.encode("utf-8"))
            return
        if path == "/app.css":
            self._asset("app.css", "text/css; charset=utf-8")
            return
        if path == "/app.js":
            self._asset("app.js", "application/javascript; charset=utf-8")
            return
        if path == "/bruce.jpg":
            self._asset("bruce.jpg", "image/jpeg")
            return

        if path == "/api/v1/system/zones":
            self._json({"ok": True, "zones": [
                {"id": 1, "name": "Door", "state": "normal", "alwaysOn": False},
                {"id": 2, "name": "Smoke", "state": "normal", "alwaysOn": True},
            ]})
            return
        if path == "/api/v1/system/partitions":
            self._json({"ok": True, "partitions": [{"id": 1, "armState": "disarmed"}]})
            return
        if path == "/api/v1/system/outputs":
            self._json({"ok": True, "outputs": [
                {"id": 1, "type": "siren", "active": False},
                {"id": 2, "type": "valve", "active": self.state.valve_active},
                {"id": 3, "type": "valve", "active": False},
            ]})
            return
        if path == "/api/v1/system/events":
            self._json({"ok": True, "events": [
                {"sequence": self.state.sequence, "event": "smoke.boot", "severity": "info"}
            ]})
            return
        if path == "/api/v1/build":
            self._json({"ok": True, "project": "HomeGuard-S3", "build": "browser-smoke"})
            return
        if path == "/api/v1/network/status":
            self._json({"ok": True, "state": "connected", "ssid": self.state.network_ssid,
                        "ip": "192.168.4.2", "rssi": -40})
            return
        if path == "/api/v1/network/scan":
            self._json({"ok": True, "networks": [{"ssid": "SmokeNet", "rssi": -42}]})
            return

        self._json({"ok": False, "reason": "not_found"}, 404)

    def _read_json(self) -> dict[str, object]:
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            length = 0
        raw = self.rfile.read(max(0, length))
        if not raw:
            return {}
        value = json.loads(raw.decode("utf-8"))
        if not isinstance(value, dict):
            raise ValueError("JSON body must be an object")
        return value

    def do_POST(self) -> None:  # noqa: N802
        path = urlparse(self.path).path
        try:
            body = self._read_json()
        except Exception:
            self.state.log("POST", path, {"_invalid": True})
            self._json({"ok": False, "reason": "invalid_json"}, 400)
            return
        self.state.log("POST", path, body)

        if path == "/api/v1/system/security-command":
            self.state.sequence += 1
            self._json({"ok": True})
            return
        if path == "/api/v1/system/output-command":
            if body.get("outputId") == 2 and isinstance(body.get("active"), bool):
                self.state.valve_active = bool(body["active"])
                self._json({"ok": True})
            else:
                self._json({"ok": False, "reason": "bad_output"}, 400)
            return
        if path == "/api/v1/network/connect":
            ssid = body.get("ssid")
            if isinstance(ssid, str) and ssid:
                self.state.network_ssid = ssid
                self._json({"ok": True, "state": "connecting", "ssid": ssid})
            else:
                self._json({"ok": False, "reason": "bad_ssid"}, 400)
            return
        if path == "/api/v1/access/users":
            action = body.get("action")
            if action == "bootstrap":
                self.state.users = [{
                    "id": body.get("id", ""), "name": body.get("name", ""),
                    "role": "admin", "enabled": True,
                }]
                self._json({"ok": True, "role": "admin", "bootstrap": True})
                return
            if action == "list":
                self._json({"ok": True, "capacity": 8, "count": len(self.state.users),
                            "users": self.state.users})
                return
            if action == "set":
                user = {"id": body.get("id", ""), "name": body.get("name", ""),
                        "role": body.get("role", "guest"), "enabled": bool(body.get("enabled", True))}
                self.state.users = [item for item in self.state.users if item.get("id") != user["id"]]
                self.state.users.append(user)
                self._json({"ok": True})
                return
            self._json({"ok": False, "reason": "unknown_action"}, 400)
            return

        self._json({"ok": False, "reason": "not_found"}, 404)


def serve(args: argparse.Namespace) -> int:
    web_root = Path(args.web).resolve()
    log_path = Path(args.log).resolve()
    for required in ("index.html", "app.css", "app.js", "bruce.jpg"):
        if not (web_root / required).is_file():
            raise SystemExit(f"missing web asset: {web_root / required}")
    server = ThreadingHTTPServer((args.host, args.port), SmokeHandler)
    server.web_root = web_root  # type: ignore[attr-defined]
    server.state = SmokeState(log_path)  # type: ignore[attr-defined]
    print(f"HomeGuard web control smoke listening on http://{args.host}:{args.port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


def verify(args: argparse.Namespace) -> int:
    log_path = Path(args.log)
    if not log_path.is_file():
        print(f"FAIL: request log not found: {log_path}")
        return 1
    records = [json.loads(line) for line in log_path.read_text(encoding="utf-8").splitlines() if line.strip()]
    posts = [record for record in records if record.get("method") == "POST"]
    errors: list[str] = []

    def bodies(path: str) -> list[dict[str, object]]:
        return [record.get("body", {}) for record in posts if record.get("path") == path]

    security = bodies("/api/v1/system/security-command")
    expected_commands = ["security.arm_away", "security.disarm", "security.arm_home", "security.panic"]
    if [item.get("command") for item in security] != expected_commands:
        errors.append(f"security command order/payload mismatch: {security}")
    for item in security:
        if item.get("actor") != "smoke-user" or item.get("credential") != "1234":
            errors.append(f"security credentials missing/wrong: {item}")

    outputs = bodies("/api/v1/system/output-command")
    expected_outputs = [
        {"outputId": 2, "active": True, "actor": "smoke-user", "credential": "1234"},
        {"outputId": 2, "active": False, "actor": "smoke-user", "credential": "1234"},
    ]
    if outputs != expected_outputs:
        errors.append(f"valve button payload mismatch: {outputs}")

    network = bodies("/api/v1/network/connect")
    expected_network = [{"ssid": "SmokeNet", "password": "password123",
                         "actor": "admin-smoke", "credential": "4321"}]
    if network != expected_network:
        errors.append(f"Wi-Fi connect payload mismatch: {network}")
    if not any(record.get("method") == "GET" and record.get("path") == "/api/v1/network/scan" for record in records):
        errors.append("Wi-Fi scan button did not call /api/v1/network/scan")

    access = bodies("/api/v1/access/users")
    if len(access) != 3:
        errors.append(f"expected bootstrap/list/set access actions, got: {access}")
    else:
        bootstrap, listing, setting = access
        if bootstrap != {"action": "bootstrap", "id": "admin-smoke", "name": "Smoke Admin", "pin": "4321"}:
            errors.append(f"first Admin bootstrap payload mismatch: {bootstrap}")
        if listing != {"actor": "admin-smoke", "credential": "4321", "action": "list"}:
            errors.append(f"access list payload mismatch: {listing}")
        if setting != {"actor": "admin-smoke", "credential": "4321", "action": "set",
                       "id": "user-smoke", "name": "Smoke User", "role": "user",
                       "pin": "5678", "enabled": True}:
            errors.append(f"access set payload mismatch: {setting}")

    if errors:
        print("Web UI control click smoke FAIL")
        for error in errors:
            print(f" - {error}")
        return 1

    print("Web UI control click smoke PASS")
    print(" - security buttons: 4/4 exact POST payloads")
    print(" - valve buttons: open + close exact POST payloads")
    print(" - Wi-Fi buttons: scan + Admin-authorized connect")
    print(" - access buttons: first Admin bootstrap + list + user save")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    serve_parser = sub.add_parser("serve")
    serve_parser.add_argument("--web", default="web")
    serve_parser.add_argument("--log", default="web-control-smoke.jsonl")
    serve_parser.add_argument("--host", default="127.0.0.1")
    serve_parser.add_argument("--port", type=int, default=8766)
    verify_parser = sub.add_parser("verify")
    verify_parser.add_argument("--log", default="web-control-smoke.jsonl")
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    raise SystemExit(serve(args) if args.command == "serve" else verify(args))
