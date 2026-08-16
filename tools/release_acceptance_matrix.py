#!/usr/bin/env python3
"""Build the cemented PR #47 acceptance matrix without conflating code and field proof.

A green static/unit/browser gate means CODE PASS only. Items that require real
ESP32-S3/Android hardware remain FIELD REQUIRED until the field session records
them. This prevents CI success from being reported as product acceptance.
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "release-acceptance-matrix.json"

GATES: dict[str, list[str]] = {
    "android_discovery": [sys.executable, "tools/check_android_discovery_contract.py"],
    "android_field": [sys.executable, "tools/check_android_field_acceptance.py"],
    "web_field": [sys.executable, "tools/check_field_acceptance_web.py"],
    "factory_reset": [sys.executable, "tools/check_factory_reset_contract.py"],
    "config_transaction": [sys.executable, "tools/check_config_transaction_contract.py"],
    "wifi_safety": [sys.executable, "tools/check_wifi_scan_safety.py"],
    "http_body": [sys.executable, "tools/check_http_post_body_safety.py"],
}


def run_gate(name: str, command: list[str]) -> dict[str, object]:
    completed = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    return {
        "name": name,
        "passed": completed.returncode == 0,
        "returnCode": completed.returncode,
        "tail": completed.stdout.strip().splitlines()[-8:],
    }


def all_pass(results: dict[str, dict[str, object]], *names: str) -> bool:
    return all(bool(results[name]["passed"]) for name in names)


def item(number: int, title: str, code_pass: bool, field_required: bool = True) -> dict[str, object]:
    return {
        "number": number,
        "title": title,
        "code": "PASS" if code_pass else "FAIL",
        "field": "REQUIRED" if field_required else "N/A",
    }


results = {name: run_gate(name, command) for name, command in GATES.items()}
A = lambda *names: all_pass(results, *names)

items = [
    item(1, "Friendly name required before save", A("android_discovery", "android_field")),
    item(2, "Normal Android list hides ID/IP/base URL", A("android_discovery", "android_field")),
    item(3, "Properties exposes technical controller details", A("android_discovery", "android_field")),
    item(4, "Only owner-assigned friendly name appears on card", A("android_discovery", "android_field")),
    item(5, "Device name remains editable", A("android_discovery", "android_field")),
    item(6, "Device can be removed from local list", A("android_discovery", "android_field")),
    item(7, "Field-proven LAN/Wi-Fi discovery is preserved", A("android_discovery", "wifi_safety")),
    item(8, "Android starts on device list including empty", A("android_field")),
    item(9, "Android never auto-opens Add Device", A("android_field")),
    item(10, "Current Add Device flow remains hidden", A("android_field")),
    item(11, "Manual IP autofill remains in hidden flow", A("android_discovery")),
    item(12, "Password/PIN show-hide controls remain available", A("android_discovery", "android_field", "web_field")),
    item(13, "Web UI has exactly one active navigation item", A("web_field")),
    item(14, "No Zones/Sensors or Inputs/Outputs double highlight", A("web_field")),
    item(15, "Phone Web menu does not cover Bruce", A("web_field")),
    item(16, "Bruce is fully visible on phone Web UI", A("web_field")),
    item(17, "Mobile Web navigation is collapsed by default", A("web_field")),
    item(18, "All acceptance fixes ship in one test revision", A("android_field", "web_field", "factory_reset", "wifi_safety", "http_body")),
    item(19, "True Factory Reset is reachable from Web and Android", A("factory_reset", "web_field", "android_field")),
    item(20, "Factory Reset erases mutable state and preserves identity/firmware", A("factory_reset", "config_transaction")),
    item(21, "Factory Reset requires explicit destructive confirmation", A("factory_reset", "web_field", "android_field")),
    item(22, "After reset/reboot controller is factory-fresh with first Admin available", A("factory_reset")),
    item(23, "Android handles reset disconnect without stale connected state", A("android_field")),
]

matrix = {
    "contract": "PR #47 cemented field acceptance 2026-08-15",
    "semantics": {
        "codePASS": "static/unit/browser gates passed",
        "fieldREQUIRED": "must still be proven on real ESP32-S3/phone before merge",
    },
    "gates": results,
    "items": items,
    "codePass": all(entry["code"] == "PASS" for entry in items),
    "fieldValidationComplete": False,
    "mergeAllowed": False,
}
OUTPUT.write_text(json.dumps(matrix, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

for entry in items:
    print(f"{entry['number']:02d}. CODE {entry['code']} · FIELD {entry['field']} · {entry['title']}")

failed_gates = [name for name, result in results.items() if not result["passed"]]
if failed_gates:
    print("\nFAILED GATES:")
    for name in failed_gates:
        print(f" - {name}")
        for line in results[name]["tail"]:
            print(f"   {line}")
    raise SystemExit(1)

print("\nCODE MATRIX PASS: all 23 items have code-level coverage.")
print("FIELD VALIDATION REMAINS REQUIRED: matrix never auto-authorizes merge.")
