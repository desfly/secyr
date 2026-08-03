from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

checks = [
    ("preflight", "tools/preflight_build0022.py"),
    ("source_audit", "tools/audit_esp_idf_sources.py"),
    ("component_audit", "tools/audit_component_dependencies.py"),
    ("gpio_safety", "tools/audit_gpio_safety.py"),
    ("firmware_budget", "tools/check_firmware_budget.py"),
    ("mock_syntax", "tools/syntax_check_esp_idf_mock.py"),
    ("mock_link", "tools/mock_link_esp_idf_main.py"),
]

results = {}
passed = True

for name, script in checks:
    run = subprocess.run(
        [sys.executable, str(ROOT / script)],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    results[name] = {
        "passed": run.returncode == 0,
        "output": (run.stdout + run.stderr).strip(),
    }
    passed = passed and run.returncode == 0
    print(f"{name}: {'PASS' if run.returncode == 0 else 'FAIL'}")

report = {
    "build": "0025",
    "release_ready_for_real_idf_compile": passed,
    "firmware_binary_present": False,
    "checks": results,
}

(ROOT / "release-readiness-build0025.json").write_text(
    json.dumps(report, indent=2),
    encoding="utf-8",
)

if not passed:
    sys.exit(1)

print("Build-0025 release readiness PASS")
