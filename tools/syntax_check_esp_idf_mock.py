from pathlib import Path
import json
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "esp-idf" / "main"
MOCK = ROOT / "tests" / "esp-idf-mock" / "include"
INCLUDE = ROOT / "firmware" / "include"

compiler = shutil.which("g++") or shutil.which("clang++")
if not compiler:
    raise SystemExit("No C++ compiler found")

results = []
failed = False

for source in sorted(MAIN.glob("*.cpp")):
    command = [
        compiler,
        "-std=c++20",
        "-fsyntax-only",
        "-I", str(MOCK),
        "-I", str(MAIN),
        "-I", str(INCLUDE),
        str(source),
    ]
    run = subprocess.run(command, capture_output=True, text=True)
    item = {
        "file": source.name,
        "returncode": run.returncode,
        "stdout": run.stdout,
        "stderr": run.stderr,
    }
    results.append(item)
    if run.returncode != 0:
        failed = True

out = ROOT / "mock-syntax-report.json"
out.write_text(json.dumps(results, indent=2), encoding="utf-8")

for item in results:
    state = "PASS" if item["returncode"] == 0 else "FAIL"
    print(f"{state}: {item['file']}")
    if item["returncode"] != 0:
        print(item["stderr"])

if failed:
    sys.exit(1)

print("ESP-IDF mock syntax check PASS")
