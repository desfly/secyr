from pathlib import Path
import hashlib
import json
import os
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: generate_firmware_manifest.py <release-dir> <output-json>")

release = Path(sys.argv[1])
output = Path(sys.argv[2])

if not release.is_dir():
    raise SystemExit(f"release directory does not exist: {release}")

files = {}
for path in sorted(release.iterdir()):
    if path.is_file() and path.name != output.name:
        data = path.read_bytes()
        files[path.name] = {
            "size": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
        }

manifest = {
    "project": "HomeGuard-S3",
    "build": "0022",
    "firmware_version": "0.22.0",
    "target": "esp32s3",
    "board": "HW-678 V0.0.0",
    "module": "ESP32-S3-WROOM-1-N16R8",
    "esp_idf": "5.4.2",
    "git_revision": os.getenv("GITHUB_SHA", "local"),
    "files": files,
}

output.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
print(output)
