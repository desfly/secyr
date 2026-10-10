"""Run behind a TLS reverse proxy; loopback binding is the default."""
import argparse
import json
import re
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from signer import Denied, Signer, load_config


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate field")
        result[key] = value
    return result


def handler_for(signer):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass  # Tokens and request bodies must never enter HTTP logs.

        def do_POST(self):
            self.connection.settimeout(5)
            route = re.fullmatch(r"/v1/devices/([A-Za-z0-9_-]+)/signed-command", self.path)
            if route is None:
                return self.reply(404, {"error": "not_found"})
            auth = self.headers.get("Authorization", "")
            if not auth.startswith("Bearer ") or not 0 < len(auth[7:]) <= 4096:
                return self.reply(401, {"error": "authorization_required"})
            if self.headers.get_content_type() != "application/json" or self.headers.get("Transfer-Encoding"):
                return self.reply(400, {"error": "invalid_request"})
            try:
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 < length <= 8192:
                    return self.reply(413, {"error": "invalid_body_size"})
                body = self.rfile.read(length)
                if len(body) != length:
                    raise ValueError("Incomplete body")
                request = json.loads(body, object_pairs_hook=unique_object)
                envelope = signer.sign(auth[7:], route[1], request)
            except Denied:
                return self.reply(403, {"error": "not_authorized"})
            except (ValueError, TypeError, UnicodeError, TimeoutError):
                return self.reply(400, {"error": "invalid_request"})
            except Exception:
                return self.reply(503, {"error": "signer_unavailable"})
            self.reply(200, envelope)

        def reply(self, status, payload):
            body = json.dumps(payload, separators=(",", ":")).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
    return Handler


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", required=True)
    parser.add_argument("--database", required=True)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9080)
    args = parser.parse_args()
    devices, grants = load_config(args.config)
    signer = Signer(args.database, devices, grants)
    ThreadingHTTPServer((args.bind, args.port), handler_for(signer)).serve_forever()
