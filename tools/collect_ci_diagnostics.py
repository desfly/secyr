from pathlib import Path
import json
import os
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
ESP = ROOT / "firmware" / "esp-idf"
BUILD = ESP / "build"
OUT = ROOT / "ci-diagnostics"
OUT.mkdir(exist_ok=True)

def run(command, cwd=None):
    result = subprocess.run(
        command,
        cwd=cwd,
        capture_output=True,
        text=True,
        shell=isinstance(command, str),
    )
    return {
        "command": command if isinstance(command, str) else " ".join(command),
        "returncode": result.returncode,
        "stdout": result.stdout,
        "stderr": result.stderr,
    }

report = {
    "python": sys.version,
    "platform": platform.platform(),
    "idf_path": os.getenv("IDF_PATH", ""),
    "github_sha": os.getenv("GITHUB_SHA", ""),
    "commands": [],
    "files": {},
}

for command in (
    ["python", "--version"],
    ["cmake", "--version"],
    ["ninja", "--version"],
    ["idf.py", "--version"],
    ["git", "rev-parse", "HEAD"],
):
    report["commands"].append(run(command, cwd=ESP))

for path in (
    ESP / "sdkconfig",
    ESP / "sdkconfig.defaults",
    ESP / "partitions.csv",
    BUILD / "compile_commands.json",
    BUILD / "project_description.json",
    BUILD / "flasher_args.json",
):
    if path.exists():
        report["files"][str(path.relative_to(ROOT))] = {
            "size": path.stat().st_size,
        }

(OUT / "diagnostics.json").write_text(
    json.dumps(report, indent=2),
    encoding="utf-8",
)

if (BUILD / "compile_commands.json").exists():
    (OUT / "compile_commands.json").write_bytes(
        (BUILD / "compile_commands.json").read_bytes()
    )

for filename in ("sdkconfig", "project_description.json", "flasher_args.json"):
    source = BUILD / filename
    if not source.exists():
        source = ESP / filename
    if source.exists():
        (OUT / filename).write_bytes(source.read_bytes())

print(OUT)
