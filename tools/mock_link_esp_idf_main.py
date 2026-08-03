from pathlib import Path
import json
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "esp-idf" / "main"
MOCK = ROOT / "tests" / "esp-idf-mock" / "include"
INCLUDE = ROOT / "firmware" / "include"
HARNESS = ROOT / "tests" / "esp-idf-mock" / "mock_main.cpp"
BUILD = ROOT / "mock-link-build"
REPORT = ROOT / "mock-link-report.json"

compiler = shutil.which("g++") or shutil.which("clang++")
if not compiler:
    raise SystemExit("No host C++ compiler found")

if BUILD.exists():
    shutil.rmtree(BUILD)
BUILD.mkdir()

cmake_text = (MAIN / "CMakeLists.txt").read_text(encoding="utf-8")
source_refs = re.findall(r'"([^"]+\.cpp)"', cmake_text)
sources = []
for ref in source_refs:
    path = (MAIN / ref).resolve()
    if path.exists():
        sources.append(path)
sources = sorted(dict.fromkeys(sources))
objects = []
results = []
failed = False

for source in sources + [HARNESS]:
    obj = BUILD / f"{source.stem}.o"
    command = [
        compiler,
        "-std=c++20",
        "-O0",
        "-g0",
        "-I", str(MOCK),
        "-I", str(MAIN),
        "-I", str(INCLUDE),
        "-c", str(source),
        "-o", str(obj),
    ]
    run = subprocess.run(command, capture_output=True, text=True)
    results.append({
        "stage": "compile",
        "file": str(source.relative_to(ROOT)),
        "returncode": run.returncode,
        "stdout": run.stdout,
        "stderr": run.stderr,
    })
    if run.returncode != 0:
        failed = True
    else:
        objects.append(obj)

link_output = BUILD / "homeguard_mock"
if not failed:
    command = [
        compiler,
        *map(str, objects),
        "-o", str(link_output),
    ]
    run = subprocess.run(command, capture_output=True, text=True)
    results.append({
        "stage": "link",
        "file": str(link_output.relative_to(ROOT)),
        "returncode": run.returncode,
        "stdout": run.stdout,
        "stderr": run.stderr,
    })
    failed = run.returncode != 0

REPORT.write_text(json.dumps(results, indent=2), encoding="utf-8")

for result in results:
    state = "PASS" if result["returncode"] == 0 else "FAIL"
    print(f"{state}: {result['stage']} {result['file']}")
    if result["returncode"] != 0:
        print(result["stderr"])

if failed:
    sys.exit(1)

print("ESP-IDF mock compile/link PASS")
