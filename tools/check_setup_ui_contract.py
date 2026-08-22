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


# Desktop setup must use the available screen instead of being locked to the
# 430 px login/mobile card.
require('#hgAuthGate.hg-setup-mode .hg-auth-card{width:min(900px,calc(100vw - 48px))}' in source,
        "desktop setup card is not widened")
require('.hg-setup-grid{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr)' in source,
        "desktop setup is not two-column")
require('@media(max-width:720px)' in source,
        "mobile breakpoint missing")
require('#hgAuthGate .hg-setup-grid{grid-template-columns:1fr;gap:14px}' in source,
        "mobile setup does not collapse to one column")
require('gate.classList.add("hg-setup-mode")' in source and 'gate.classList.remove("hg-setup-mode")' in source,
        "setup/login responsive mode switch missing")

# Wi-Fi scan results must be immediately visible; no second select/dropdown tap.
require('id="hgSetupWifiNetworks"' in source and 'role="listbox"' in source,
        "visible Wi-Fi result list missing")
require('row.className="hg-setup-network"' in source,
        "scan results are not rendered as selectable rows")
require('list.appendChild(row)' in source,
        "scan results are not appended to the visible list")
require('row.onclick=()=>{' in source and 'form.querySelector("#hgSetupWifiPassword")?.focus()' in source,
        "choosing an AP does not select SSID and advance to password")
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
    '.hg-password-label input{padding-right:52px!important}',
    'button.hg-password-eye{position:absolute!important',
    'button.innerHTML = \'<svg viewBox="0 0 24 24" aria-hidden="true">',
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

print("Setup UI contract PASS (source only; hardware validation still required)")
print(" - desktop: wide two-column setup")
print(" - mobile: single-column setup at <=720px")
print(" - Wi-Fi scan: visible AP rows, no dropdown")
print(" - password eyes: source-owned controls for Wi-Fi, Admin PIN and login PIN")
print(" - legacy firmware-layer password-eye patch: absent")