#!/usr/bin/env python3
"""Regression gate for the first-boot setup UI.

This is a source/contract check only. It never upgrades the bug status to
HW PASS/FIXED; real browser validation on flashed hardware is still required.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "web" / "access-session.js").read_text(encoding="utf-8")
embedded = (ROOT / "firmware" / "esp-idf" / "main" / "hg_web_http.cpp").read_text(encoding="utf-8")
errors: list[str] = []


def require(condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


# Source ownership and compatibility boundary. Layout, visibility, selection and
# click behavior are verified by setup_eye_runtime_smoke.py in a real browser.
require('id="hgSetupWifiNetworks"' in source and 'role="listbox"' in source,
        "setup source no longer owns the Wi-Fi result list")
require('<select id="hgSetupWifiSsid"' not in source,
        "old Wi-Fi dropdown returned")
require('id="hgSetupWifiSsid" type="text"' in source,
        "manual/hidden SSID fallback missing")

# Password visibility controls must live with the dynamic auth/setup form source,
# not as a firmware-layer patch. This still does not prove rendered behavior.
for required in (
    'function attachPasswordEye(',
    'className = "hg-password-eye"',
    'attachPasswordEye("hgSetupWifiPassword", "Показати пароль Wi-Fi")',
    'attachPasswordEye("hgSetupPin", "Показати пароль / PIN")',
    'attachPasswordEye("hgLoginPin", "Показати пароль / PIN")',
):
    require(required in source, f"source-owned password visibility control missing: {required}")

for forbidden in (
    'attachPasswordToggle(',
    'ensurePasswordToggles',
    'passwordToggleObserver',
):
    require(forbidden not in embedded,
            f"legacy firmware-layer password-eye patch still present: {forbidden}")

if errors:
    print("Setup UI contract FAIL", file=sys.stderr)
    for error in errors:
        print(f" - {error}", file=sys.stderr)
    raise SystemExit(1)

print("Setup UI source boundary PASS (hardware validation still required)")
print(" - setup source owns Wi-Fi list and manual SSID fallback")
print(" - setup source owns password eyes for Wi-Fi, Admin PIN and login PIN")
print(" - legacy firmware-layer password-eye patch: absent")
