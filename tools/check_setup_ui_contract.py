#!/usr/bin/env python3
"""Regression gate for the first-boot setup UI."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "web" / "access-session.js").read_text(encoding="utf-8")
css = (ROOT / "web" / "app.css").read_text(encoding="utf-8")
errors: list[str] = []


def require(condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


# Base setup markup/runtime contract.
require('#hgAuthGate.hg-setup-mode .hg-auth-card{width:min(900px,calc(100vw - 48px))}' in source,
        "desktop setup card base width missing")
require('.hg-setup-grid{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr)' in source,
        "desktop setup base is not two-column")
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

# CSS must have one stable responsive strategy instead of the historical stack
# of mutually overriding 52/57/58/59vw setup patches.
require("First-boot setup: one stable responsive definition." in css,
        "stable setup CSS contract marker missing")
for forbidden in (
    "width:57vw!important",
    "width:58vw!important",
    "width:59vw!important",
    "max-height:none!important;overflow:visible!important",
    "Maximum 1080p setup size",
    "First-boot Wi-Fi scan: show every discovered network, no inner scrollbar",
):
    require(forbidden not in css, f"obsolete stacked setup override returned: {forbidden}")

# Large desktop target: compact top-left card, Bruce bounded on the right,
# readable controls, and a bounded two-column Wi-Fi list.
for required in (
    "width:clamp(720px,48vw,880px)!important",
    "width:min(44vw,760px)!important",
    "font-size:32px!important",
    "font-size:17px!important",
    "grid-template-columns:minmax(0,1fr) minmax(0,1fr)!important",
    "max-height:240px!important",
    "overflow-y:auto!important",
    "overflow-x:hidden!important",
):
    require(required in css, f"desktop setup CSS contract missing: {required}")

# Long SSIDs must shrink inside their grid cell instead of widening the list.
for required in (
    ".hg-setup-network{min-width:0!important;overflow:hidden!important}",
    ".hg-setup-network strong{min-width:0!important;flex:1 1 auto!important;overflow:hidden!important;text-overflow:ellipsis!important;white-space:nowrap!important}",
    ".hg-setup-network span{flex:0 0 auto!important}",
):
    require(required in css, f"Wi-Fi row overflow guard missing: {required}")

# User-visible state dots are intentionally larger than the old 10px markers.
require(".side-foot i{display:inline-block;width:12px;height:12px" in css,
        "sidebar online dot is not enlarged")
require(".events i{width:12px;height:12px" in css,
        "event dots are not enlarged")
require(".zone:before{content:'●';color:#0aaa42;margin-right:12px;font-size:18px" in css,
        "zone status dot is not enlarged")

if errors:
    print("Setup UI contract FAIL", file=sys.stderr)
    for error in errors:
        print(f" - {error}", file=sys.stderr)
    raise SystemExit(1)

print("Setup UI contract PASS")
print(" - desktop: compact stable setup card with larger readable controls")
print(" - Bruce: bounded right-side illustration")
print(" - mobile/tablet: deterministic responsive collapse")
print(" - Wi-Fi scan: visible bounded AP rows, no horizontal overflow")
print(" - status dots: enlarged for readability")
