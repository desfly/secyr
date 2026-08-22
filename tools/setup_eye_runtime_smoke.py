#!/usr/bin/env python3
"""Runtime browser smoke test for first-boot password visibility controls.

Serves the real checked-in Web UI, forces /api/v1/access/state to setup_required,
and injects a tiny browser harness that verifies the Wi-Fi/Admin password eyes are
actually rendered, visible, and toggle input type password <-> text.

This is CI/browser proof only, not HW PASS. Final acceptance still requires the
flashed ESP32-S3 in the real browser.
"""
from __future__ import annotations

import argparse
import json
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

  const verifyEye = async inputSelector => {
    const input = await waitFor(inputSelector);
    const label = input.closest('label');
    if (!label) throw new Error(`${inputSelector}: label missing`);
    const eye = label.querySelector('button.hg-password-eye');
    if (!eye) throw new Error(`${inputSelector}: eye missing`);

    const style = getComputedStyle(eye);
    const rect = eye.getBoundingClientRect();
    if (style.display === 'none' || style.visibility === 'hidden' || Number(style.opacity) === 0)
      throw new Error(`${inputSelector}: eye hidden`);
    if (rect.width < 24 || rect.height < 24)
      throw new Error(`${inputSelector}: eye too small ${rect.width}x${rect.height}`);

    if (input.type !== 'password') throw new Error(`${inputSelector}: expected password before click`);
    eye.click();
    await sleep(40);
    if (input.type !== 'text') throw new Error(`${inputSelector}: click did not reveal password`);
    eye.click();
    await sleep(40);
    if (input.type !== 'password') throw new Error(`${inputSelector}: second click did not hide password`);
  };

  await verifyEye('#hgSetupWifiPassword');
  await verifyEye('#hgSetupPin');

  document.documentElement.dataset.setupEyeSmoke = 'done';
  document.documentElement.dataset.setupEyeCount = String(document.querySelectorAll('button.hg-password-eye').length);
})().catch(error => {
  document.documentElement.dataset.setupEyeSmoke = 'failed';
  document.documentElement.dataset.setupEyeError = String(error && error.message || error);
});
</script>
"""


class Handler(BaseHTTPRequestHandler):
    server_version = "HomeGuardSetupEyeSmoke/1.0"

    @property
    def web_root(self) -> Path:
        return self.server.web_root  # type: ignore[attr-defined]

    def log_message(self, fmt: str, *args: object) -> None:
        return

    def _send(self, status: int, content_type: str, payload: bytes) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def _json(self, body: object, status: int = 200) -> None:
        self._send(status, "application/json; charset=utf-8",
                   json.dumps(body, ensure_ascii=False, separators=(",", ":")).encode("utf-8"))

    def _asset(self, name: str, content_type: str) -> None:
        path = self.web_root / name
        if not path.is_file():
            self._json({"ok": False, "reason": "asset_not_found"}, 404)
            return
        self._send(200, content_type, path.read_bytes())

    def do_GET(self) -> None:  # noqa: N802
        path = urlparse(self.path).path
        if path in ("/", "/index.html"):
            html = (self.web_root / "index.html").read_text(encoding="utf-8")
            html = html.replace("</body>", HARNESS + "\n</body>")
            self._send(200, "text/html; charset=utf-8", html.encode("utf-8"))
            return
        if path == "/api/v1/access/state":
            self._json({"ok": True, "state": "setup_required"})
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
        self._json({"ok": False, "reason": "not_found"}, 404)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--web", type=Path, default=Path("web"))
    parser.add_argument("--port", type=int, default=8771)
    args = parser.parse_args()

    root = args.web.resolve()
    if not (root / "index.html").is_file() or not (root / "access-session.js").is_file():
        raise SystemExit(f"invalid web root: {root}")

    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    server.web_root = root  # type: ignore[attr-defined]
    print(f"setup-eye smoke server: http://127.0.0.1:{args.port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
