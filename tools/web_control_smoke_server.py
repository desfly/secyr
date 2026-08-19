#!/usr/bin/env python3
"""Headless-browser control smoke server for the HomeGuard-S3 Web UI.

Serves the checked-in Web UI with deterministic mock API responses and injects
one browser harness that exercises the current full-screen access-session gate,
role gating and high-priority controls. ``verify`` checks the captured HTTP
requests so a DOM-only success cannot hide missing control traffic.
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
  const waitFor = async (selector, timeout = 7000) => {
    const deadline = Date.now() + timeout;
    while (Date.now() < deadline) {
      const item = document.querySelector(selector);
      if (item) return item;
      await sleep(50);
    }
    throw new Error(`timeout waiting for ${selector}`);
  };
  const waitUntil = async (test, label, timeout = 7000) => {
    const deadline = Date.now() + timeout;
    while (Date.now() < deadline) {
      if (test()) return;
      await sleep(50);
    }
    throw new Error(`timeout waiting for ${label}`);
  };
  const waitEnabled = async (selector, timeout = 7000) => {
    const item = await waitFor(selector, timeout);
    await waitUntil(() => !item.disabled, `enabled ${selector}`, timeout);
    return item;
  };
  const login = async (actor, pin) => {
    const gate = await waitFor('#hgAuthGate');
    const actorField = await waitFor('#hgLoginActor');
    const pinField = await waitFor('#hgLoginPin');
    actorField.value = actor;
    pinField.value = pin;
    (await waitEnabled('#hgAuthForm button[type="submit"]')).click();
    await waitUntil(() => gate.hidden === true, `auth gate hidden for ${actor}`);
    await waitFor('#hgSessionLogout');
    await sleep(250);
  };
  const logout = async () => {
    (await waitEnabled('#hgSessionLogout')).click();
    await waitUntil(() => document.querySelector('#hgAuthGate')?.hidden === false, 'auth gate visible');
    await waitFor('#hgLoginActor');
    await sleep(120);
  };

  await waitFor('html[data-homeguard-ui="ready"]');
  await waitFor('[data-output-id="2"][data-output-active="true"]');

  // Factory-fresh bootstrap through the actual access-session setup gate.
  document.querySelector('#hgSetupId').value = 'admin-smoke';
  document.querySelector('#hgSetupName').value = 'Smoke Admin';
  document.querySelector('#hgSetupPin').value = '4321';
  (await waitEnabled('#hgAuthForm button[type="submit"]')).click();
  await waitFor('#hgLoginActor');
  await sleep(180);

  // Admin: full access, including Panic, Wi-Fi and account management.
  await login('admin-smoke', '4321');
  (await waitEnabled('[data-command="security.panic"]')).click();
  await sleep(450);

  (await waitEnabled('#wifiScan')).click();
  await sleep(350);
  document.querySelector('#wifiSsid').value = 'SmokeNet';
  document.querySelector('#wifiPassword').value = 'password123';
  (await waitEnabled('#wifiConnect')).click();
  await sleep(2100);

  (await waitEnabled('#accessLoad')).click();
  await sleep(350);

  document.querySelector('#managedUserId').value = 'smoke-user';
  document.querySelector('#managedUserName').value = 'Smoke User';
  document.querySelector('#managedUserRole').value = 'user';
  document.querySelector('#managedUserPin').value = '1234';
  document.querySelector('#managedUserEnabled').checked = true;
  (await waitEnabled('#accessSave')).click();
  await sleep(350);

  document.querySelector('#managedUserId').value = 'guest-smoke';
  document.querySelector('#managedUserName').value = 'Smoke Guest';
  document.querySelector('#managedUserRole').value = 'guest';
  document.querySelector('#managedUserPin').value = '6789';
  document.querySelector('#managedUserEnabled').checked = true;
  (await waitEnabled('#accessSave')).click();
  await sleep(350);
  await logout();

  // User: arm/disarm + valves, but no Panic/network/account management.
  await login('smoke-user', '1234');
  for (const command of ['security.arm_away', 'security.disarm', 'security.arm_home']) {
    (await waitEnabled(`[data-command="${command}"]`)).click();
    await sleep(450);
  }
  for (const selector of ['[data-command="security.panic"]', '#wifiConnect', '#accessSave']) {
    const item = await waitFor(selector);
    if (!item.disabled) throw new Error(`${selector} unexpectedly enabled for user`);
  }
  (await waitEnabled('[data-output-id="2"][data-output-active="true"]')).click();
  await sleep(600);
  (await waitEnabled('[data-output-id="2"][data-output-active="false"]')).click();
  await sleep(600);
  await logout();

  // Guest: monitoring only; every command control remains disabled.
  await login('guest-smoke', '6789');
  for (const selector of [
    '[data-command="security.arm_away"]',
    '[data-command="security.disarm"]',
    '[data-command="security.arm_home"]',
    '[data-command="security.panic"]',
    '[data-output-id="2"][data-output-active="true"]',
    '#wifiConnect', '#accessLoad', '#accessSave'
  ]) {
    const item = await waitFor(selector);
    if (!item.disabled) throw new Error(`${selector} unexpectedly enabled for guest`);
  }

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
        self.credentials: dict[str, str] = {}
        self.sequence = 1
        self.log_path.write_text("", encoding="utf-8")

    def log(self, method: str, path: str, body: object | None = None) -> None:
        record: dict[str, object] = {"method": method, "path": path}
        if body is not None:
            record["body"] = body
        with self.lock:
            with self.log_path.open("a", encoding="utf-8") as handle:
                handle.write(json.dumps(record, ensure_ascii=False, sort_keys=True) + "\n")

    def find_user(self, actor: object) -> dict[str, object] | None:
        if not isinstance(actor, str):
            return None
        return next((u for u in self.users if u.get("id") == actor and u.get("enabled") is True), None)


class SmokeHandler(BaseHTTPRequestHandler):
    server_version = "HomeGuardWebSmoke/2.0"

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
        self._send(status, "application/json; charset=utf-8",
                   json.dumps(body, ensure_ascii=False, separators=(",", ":")).encode("utf-8"))

    def _asset(self, filename: str, content_type: str) -> None:
        path = self.web_root / filename
        if not path.is_file():
            self._json({"ok": False, "reason": "asset_not_found"}, 404)
            return
        self._send(200, content_type, path.read_bytes())

    @staticmethod
    def _capabilities(role: str) -> dict[str, bool]:
        admin = role == "admin"
        user = role == "user"
        return {
            "monitor": True,
            "armHome": admin or user,
            "armAway": admin or user,
            "disarm": admin or user,
            "panic": admin,
            "valves": admin or user,
            "networkConfigure": admin,
            "accessManage": admin,
            "serviceInvalidate": admin,
        }

    def do_GET(self) -> None:  # noqa: N802
        path = urlparse(self.path).path
        self.state.log("GET", path)
        if path in ("/", "/index.html"):
            html = (self.web_root / "index.html").read_text(encoding="utf-8")
            self._send(200, "text/html; charset=utf-8", html.replace("</body>", HARNESS + "\n</body>").encode("utf-8"))
            return
        assets = {
            "/app.css": ("app.css", "text/css; charset=utf-8"),
            "/app.js": ("app.js", "application/javascript; charset=utf-8"),
            "/access-session.js": ("access-session.js", "application/javascript; charset=utf-8"),
            "/factory-reset.js": ("factory-reset.js", "application/javascript; charset=utf-8"),
            "/bruce.jpg": ("bruce.jpg", "image/jpeg"),
        }
        if path in assets:
            self._asset(*assets[path])
            return
        if path == "/api/v1/access/state":
            self._json({"ok": True, "state": "setup_required" if not self.state.users else "login_required"})
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
            self._json({"ok": True, "events": [{"sequence": self.state.sequence, "event": "smoke.boot", "severity": "info"}]})
            return
        if path == "/api/v1/build":
            self._json({"ok": True, "project": "HomeGuard-S3", "build": "browser-smoke"})
            return
        if path == "/api/v1/network/status":
            self._json({"ok": True, "state": "connected", "ssid": self.state.network_ssid, "ip": "192.168.4.2", "rssi": -40})
            return
        if path == "/api/v1/network/scan":
            self._json({"ok": True, "networks": [{"ssid": "SmokeNet", "rssi": -42}]})
            return
        if path == "/api/v1/cloud/status":
            self._json({"ok": True, "configured": False, "connected": False, "deviceId": "smoke", "connectCount": 0})
            return
        if path in ("/api/v1/lan/devices", "/api/v1/lan/scan"):
            self._json({"ok": True, "state": "online", "devices": [], "activeScan": path.endswith("/scan")})
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

        if path == "/api/v1/access/login":
            actor = body.get("actor")
            credential = body.get("credential")
            user = self.state.find_user(actor)
            if user is None or not isinstance(actor, str) or self.state.credentials.get(actor) != credential:
                self._json({"ok": False, "reason": "denied_credential"}, 401)
                return
            role = str(user.get("role", "guest"))
            self._json({
                "ok": True, "actor": actor, "name": user.get("name", actor), "role": role,
                "sessionToken": "a" * 64, "capabilities": self._capabilities(role),
            })
            return
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
                user_id = str(body.get("id", ""))
                self.state.users = [{"id": user_id, "name": body.get("name", ""), "role": "admin", "enabled": True}]
                self.state.credentials[user_id] = str(body.get("pin", ""))
                self._json({"ok": True, "role": "admin", "bootstrap": True})
                return
            if action == "list":
                self._json({"ok": True, "capacity": 8, "count": len(self.state.users), "users": self.state.users})
                return
            if action == "set":
                user_id = str(body.get("id", ""))
                user = {"id": user_id, "name": body.get("name", ""), "role": body.get("role", "guest"), "enabled": bool(body.get("enabled", True))}
                self.state.users = [u for u in self.state.users if u.get("id") != user_id]
                self.state.users.append(user)
                self.state.credentials[user_id] = str(body.get("pin", ""))
                self._json({"ok": True})
                return
            self._json({"ok": False, "reason": "unknown_action"}, 400)
            return
        self._json({"ok": False, "reason": "not_found"}, 404)


def serve(args: argparse.Namespace) -> int:
    web_root = Path(args.web).resolve()
    log_path = Path(args.log).resolve()
    for required in ("index.html", "app.css", "app.js", "access-session.js", "bruce.jpg"):
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
    posts = [r for r in records if r.get("method") == "POST"]
    errors: list[str] = []

    def bodies(path: str) -> list[dict[str, object]]:
        return [r.get("body", {}) for r in posts if r.get("path") == path]

    expected_logins = [
        {"actor": "admin-smoke", "credential": "4321"},
        {"actor": "smoke-user", "credential": "1234"},
        {"actor": "guest-smoke", "credential": "6789"},
    ]
    if bodies("/api/v1/access/login") != expected_logins:
        errors.append(f"login order/payload mismatch: {bodies('/api/v1/access/login')}")

    expected_security = [
        {"command": "security.panic", "actor": "admin-smoke", "credential": "4321"},
        {"command": "security.arm_away", "actor": "smoke-user", "credential": "1234"},
        {"command": "security.disarm", "actor": "smoke-user", "credential": "1234"},
        {"command": "security.arm_home", "actor": "smoke-user", "credential": "1234"},
    ]
    if bodies("/api/v1/system/security-command") != expected_security:
        errors.append(f"security role/payload mismatch: {bodies('/api/v1/system/security-command')}")

    expected_outputs = [
        {"outputId": 2, "active": True, "actor": "smoke-user", "credential": "1234"},
        {"outputId": 2, "active": False, "actor": "smoke-user", "credential": "1234"},
    ]
    if bodies("/api/v1/system/output-command") != expected_outputs:
        errors.append(f"valve button payload mismatch: {bodies('/api/v1/system/output-command')}")

    expected_network = [{"ssid": "SmokeNet", "password": "password123", "actor": "admin-smoke", "credential": "4321"}]
    if bodies("/api/v1/network/connect") != expected_network:
        errors.append(f"Wi-Fi connect payload mismatch: {bodies('/api/v1/network/connect')}")
    if not any(r.get("method") == "GET" and r.get("path") == "/api/v1/network/scan" for r in records):
        errors.append("Wi-Fi scan button did not call /api/v1/network/scan")

    access = bodies("/api/v1/access/users")
    if len(access) != 4:
        errors.append(f"expected bootstrap/list/user-set/guest-set actions, got: {access}")
    else:
        bootstrap, listing, user_set, guest_set = access
        if bootstrap != {"action": "bootstrap", "id": "admin-smoke", "name": "Smoke Admin", "pin": "4321"}:
            errors.append(f"first Admin bootstrap payload mismatch: {bootstrap}")
        if listing != {"actor": "admin-smoke", "credential": "4321", "action": "list"}:
            errors.append(f"access list payload mismatch: {listing}")
        if user_set != {"actor": "admin-smoke", "credential": "4321", "action": "set", "id": "smoke-user", "name": "Smoke User", "role": "user", "pin": "1234", "enabled": True}:
            errors.append(f"user set payload mismatch: {user_set}")
        if guest_set != {"actor": "admin-smoke", "credential": "4321", "action": "set", "id": "guest-smoke", "name": "Smoke Guest", "role": "guest", "pin": "6789", "enabled": True}:
            errors.append(f"guest set payload mismatch: {guest_set}")

    if errors:
        print("Web UI role/control smoke FAIL")
        for error in errors:
            print(f" - {error}")
        return 1
    print("Web UI role/control smoke PASS")
    print(" - access-session setup/login/logout lifecycle: exercised")
    print(" - Admin: Panic + Wi-Fi + user management")
    print(" - User: arm/disarm + valve open/close; restricted Admin controls")
    print(" - Guest: monitoring-only controls remain disabled")
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
