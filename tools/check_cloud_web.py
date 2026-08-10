#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
HTML = ROOT / "web" / "cloud" / "index.html"

REQUIRED_MARKERS = (
    "profile.self",
    "sensors.status",
    "access.users.list",
    "access.users.upsert",
    "access.users.delete",
    "canonicalTarget",
    "canonicalUpsert",
    "ADMIN-PAYLOAD",
    "ADMIN-NEW-PIN",
    "can_manage_users",
)


def fail(message: str) -> None:
    print(f"cloud-web gate: FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    if not HTML.is_file():
        fail(f"missing {HTML.relative_to(ROOT)}")

    text = HTML.read_text(encoding="utf-8")
    for marker in REQUIRED_MARKERS:
        if marker not in text:
            fail(f"missing required marker: {marker}")

    scripts = re.findall(r"<script(?![^>]*\bsrc=)[^>]*>(.*?)</script>", text, flags=re.I | re.S)
    if len(scripts) != 1:
        fail(f"expected exactly one inline script, found {len(scripts)}")

    node = shutil.which("node")
    if not node:
        fail("node executable not found")

    with tempfile.TemporaryDirectory(prefix="homeguard-cloud-web-") as td:
        js = pathlib.Path(td) / "cloud-dashboard.js"
        js.write_text(scripts[0], encoding="utf-8")
        proc = subprocess.run(
            [node, "--check", str(js)],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        if proc.returncode != 0:
            if proc.stdout:
                print(proc.stdout, file=sys.stderr)
            if proc.stderr:
                print(proc.stderr, file=sys.stderr)
            fail("JavaScript syntax check failed")

    print("cloud-web gate: PASS")


if __name__ == "__main__":
    main()
