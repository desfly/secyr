#!/usr/bin/env python3
"""Tiny local-only preview server for deterministic first-boot screenshots."""
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / "web"

NETWORKS = [
    {"ssid": "Xiaomi_296F", "rssi": -50},
    {"ssid": "Nitros", "rssi": -60},
    {"ssid": "omega", "rssi": -74},
    {"ssid": "Kyivstar-C160", "rssi": -75},
    {"ssid": "Xiaomi_501B", "rssi": -78},
    {"ssid": "HUAWEI", "rssi": -82},
    {"ssid": "TP-Link_8C30", "rssi": -87},
    {"ssid": "WIFI", "rssi": -88},
    {"ssid": "Elvira1", "rssi": -91},
    {"ssid": "TP-Link_B268", "rssi": -91},
    {"ssid": "Lakky", "rssi": -91},
    {"ssid": "TP-Link_9840", "rssi": -92},
    {"ssid": "Volia_91", "rssi": -95},
]


class PreviewHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB), **kwargs)

    def _json(self, payload: dict) -> None:
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self) -> None:  # noqa: N802
        path = self.path.split("?", 1)[0]
        if path == "/api/v1/access/state":
            self._json({"ok": True, "state": "setup_required"})
            return
        if path == "/api/v1/network/scan":
            self._json({"ok": True, "networks": NETWORKS})
            return
        if path in ("/", "/index.html"):
            html = (WEB / "index.html").read_text(encoding="utf-8")
            preview_script = """
<script>
window.addEventListener('load',()=>setTimeout(()=>document.querySelector('#hgSetupWifiScan')?.click(),500));
</script>
"""
            html = html.replace("</body>", preview_script + "</body>")
            data = html.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        super().do_GET()

    def log_message(self, fmt: str, *args) -> None:
        sys.stderr.write("preview: " + (fmt % args) + "\n")


def main() -> None:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    server = ThreadingHTTPServer(("127.0.0.1", port), PreviewHandler)
    print(f"Web UI preview server listening on 127.0.0.1:{port}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
