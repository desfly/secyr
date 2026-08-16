#!/usr/bin/env python3
"""Guard controller POST handlers against TCP-fragment body truncation.

ESP-IDF httpd_req_recv() may return less than content_len. Every mutating or
credential-bearing POST endpoint must therefore loop until the declared body is
fully consumed or fail closed. Output-command scalar parsing is also required
to accept normal JSON whitespace after ':' rather than a single compact form.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "esp-idf" / "main"

sources = {
    "access": (MAIN / "hg_access_http.cpp").read_text(encoding="utf-8"),
    "cloud": (MAIN / "hg_cloud_http.cpp").read_text(encoding="utf-8"),
    "config": (MAIN / "hg_config_http.cpp").read_text(encoding="utf-8"),
    "network": (MAIN / "hg_network_http.cpp").read_text(encoding="utf-8"),
    "output": (MAIN / "hg_output_http.cpp").read_text(encoding="utf-8"),
    "service": (MAIN / "hg_service_http.cpp").read_text(encoding="utf-8"),
    "system": (MAIN / "hg_system_http.cpp").read_text(encoding="utf-8"),
    "telemetry": (MAIN / "hg_telemetry_session_http.cpp").read_text(encoding="utf-8"),
}

checks = {
    "access loops body": "while (offset < body.size())" in sources["access"],
    "cloud loops body": "while (offset < body.size())" in sources["cloud"] and "read_request_body(request, 1024U, body)" in sources["cloud"],
    "config loops body": "while (received_total < body.size())" in sources["config"],
    "network loops body": "while (offset < body.size())" in sources["network"] and "read_request_body(request, 384U, body)" in sources["network"],
    "output loops body": "while (offset < body.size())" in sources["output"] and "read_request_body(request, 384U, body)" in sources["output"],
    "service loops body": "while (offset < body.size())" in sources["service"],
    "system loops body": "while (offset < body.size())" in sources["system"] and "read_request_body(request, 512U, body)" in sources["system"],
    "telemetry loops body": "while (offset < body.size())" in sources["telemetry"],
    "output scalar parser finds colon independently": "find_json_value" in sources["output"] and "body.find(':', pos + marker.size())" in sources["output"],
    "output scalar parser skips JSON whitespace": "std::isspace(static_cast<unsigned char>(body[pos]))" in sources["output"],
    "output uint uses tolerant value locator": "if (!find_json_value(body, key, pos)) return false;" in sources["output"],
}

# Catch the exact anti-pattern that caused the field-risk: allocate content_len,
# perform one recv into the whole buffer, then resize to that first fragment.
for name, source in sources.items():
    single_recv = "const auto received = httpd_req_recv(request, body.data(), body.size());" in source
    resize_first_fragment = "body.resize(static_cast<std::size_t>(received));" in source or "body.resize(static_cast<size_t>(received));" in source
    checks[f"{name} has no single-fragment body pattern"] = not (single_recv and resize_first_fragment)

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(("OK   " if ok else "FAIL ") + name)

if failed:
    raise SystemExit("HTTP POST body safety failed: " + ", ".join(failed))

print("HTTP POST body safety PASS")
print(" - all credential-bearing/mutating POST handlers consume complete bodies")
print(" - single-fragment recv+resize anti-pattern is forbidden")
print(" - output scalar JSON accepts normal whitespace")
