#!/usr/bin/env python3
"""Regression gate for the first-boot setup UI."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "web" / "access-session.js").read_text(encoding="utf-8")
css = (ROOT / "web" / "app.css").read_text(encoding="utf-8")
html = (ROOT / "web" / "index.html").read_text(encoding="utf-8")
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

# Approved desktop setup shows the complete Wi-Fi scan list. The injected
# access-session defaults may contain a max-height, but app.css must override
# it on desktop so there is no inner scrollbar.
require('First-boot Wi-Fi scan: show every discovered network, no inner scrollbar.' in css,
        "approved no-inner-scroll Wi-Fi override missing")
require('html body #hgAuthGate.hg-setup-mode .hg-auth-stage:has(.hg-setup-grid){overflow:visible!important}' in css,
        "setup stage does not allow full Wi-Fi list height")
require('html body #hgAuthGate.hg-setup-mode .hg-auth-card:has(.hg-setup-grid) .hg-setup-networks{max-height:none!important;overflow:visible!important}' in css,
        "Wi-Fi list inner scrolling is not disabled")

# Password visibility must stay available in login and first-boot setup without
# changing authentication semantics or persisting the secret.
for password_id in ("hgLoginPin", "hgSetupWifiPassword", "hgSetupPin"):
    require(password_id in html,
            f"password visibility eye target missing from UI helper: {password_id}")
require('const passwordIds = ["hgLoginPin", "hgSetupWifiPassword", "hgSetupPin"]' in html,
        "approved password eye target set changed")
require('input.type = reveal ? "text" : "password"' in html,
        "password eye does not toggle password/text visibility")
require('aria-label", reveal ? "Сховати пароль" : "Показати пароль"' in html,
        "password eye accessible show/hide state missing")
require('eye.textContent = "👁"' in html,
        "visible eye control missing")
require('new MutationObserver(wirePasswordEyes)' in html,
        "dynamic login/setup password fields are not watched for eye attachment")
require('input.dataset.hgPasswordEye = "1"' in html,
        "password eye duplicate-attachment guard missing")

if errors:
    print("Setup UI contract FAIL", file=sys.stderr)
    for error in errors:
        print(f" - {error}", file=sys.stderr)
    raise SystemExit(1)

print("Setup UI contract PASS")
print(" - desktop: wide two-column setup")
print(" - mobile: single-column setup at <=720px")
print(" - Wi-Fi scan: full visible AP list, no inner scrollbar")
print(" - password visibility: eye toggles present for login, Wi-Fi and first Admin PIN")
