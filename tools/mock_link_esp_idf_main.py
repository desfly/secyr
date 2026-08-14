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
CORE_SRC = ROOT / "firmware" / "src"
COMPONENTS = ROOT / "firmware" / "esp-idf" / "components"
HARNESS = ROOT / "tests" / "esp-idf-mock" / "mock_main.cpp"
BUILD = ROOT / "mock-link-build"
REPORT = ROOT / "mock-link-report.json"

compiler = shutil.which("g++") or shutil.which("clang++")
if not compiler:
    raise SystemExit("No host C++ compiler found")

if BUILD.exists():
    shutil.rmtree(BUILD)
BUILD.mkdir()

component_include_args = []
if COMPONENTS.exists():
    for include_dir in sorted(COMPONENTS.glob("*/include")):
        component_include_args.extend(["-I", str(include_dir)])

sources = []

def add_cmake_sources(cmake_path: Path, base_dir: Path) -> None:
    if not cmake_path.exists():
        return
    cmake_text = cmake_path.read_text(encoding="utf-8")
    for ref in re.findall(r'"([^"]+\.cpp)"', cmake_text):
        path = (base_dir / ref).resolve()
        if path.exists():
            sources.append(path)

add_cmake_sources(MAIN / "CMakeLists.txt", MAIN)

# Link the implementations of ESP-IDF components that app_main directly uses.
for component_name in ("nvs_config_store", "websocket_telemetry"):
    component_dir = COMPONENTS / component_name
    add_cmake_sources(component_dir / "CMakeLists.txt", component_dir)

# These core implementations back the live telemetry path but are not all
# listed as main component sources because ESP-IDF normally gets them through
# the homeguard_core component dependency.
for filename in ("provisioning.cpp", "health_monitor.cpp", "telemetry.cpp", "local_api.cpp"):
    path = CORE_SRC / filename
    if path.exists():
        sources.append(path.resolve())

sources = sorted(dict.fromkeys(sources))
objects = []
results = []
failed = False

for index, source in enumerate(sources + [HARNESS]):
    obj = BUILD / f"{index:03d}_{source.stem}.o"
    command = [
        compiler,
        "-std=c++20",
        "-O0",
        "-g0",
        "-I", str(MOCK),
        "-I", str(MAIN),
        "-I", str(INCLUDE),
        *component_include_args,
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
