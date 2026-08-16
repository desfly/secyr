#!/usr/bin/env python3
"""Static field gate for the cemented Android acceptance rules."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
ANDROID = ROOT / "android/app/src/main/java/ua/homeguard/s3"
list_screen = (ANDROID / "ui/screens/DeviceListScreen.kt").read_text(encoding="utf-8")
add_screen = (ANDROID / "ui/screens/AddDeviceScreen.kt").read_text(encoding="utf-8")
store = (ANDROID / "storage/RegisteredDeviceStore.kt").read_text(encoding="utf-8")
settings = (ANDROID / "storage/SettingsStore.kt").read_text(encoding="utf-8")
navigation = (ANDROID / "navigation/AppNavigation.kt").read_text(encoding="utf-8")
maintenance = (ANDROID / "ControllerMaintenanceActivity.kt").read_text(encoding="utf-8")
client = (ANDROID / "diagnostics/DeviceConfigMaintenanceClient.kt").read_text(encoding="utf-8")
errors: list[str] = []


def require(condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


# Device list is the primary screen and old Add flow is hidden.
require("AppNavigation(initial: AppScreen = AppScreen.DEVICE_LIST)" in navigation,
        "Android does not start on DEVICE_LIST")
require('Text("+ Додати")' not in list_screen and 'Text("+ Додати пристрій")' not in list_screen,
        "old Add Device action is visible in main list")
require("Додавання пристроїв тимчасово сховане" in list_screen,
        "empty list does not preserve hidden-Add contract")

# Friendly names only on cards; technical data belongs in Properties.
require('var deviceName by remember { mutableStateOf("") }' in add_screen,
        "new device name does not start empty")
require("val nameValid = cleanName.isNotBlank()" in add_screen,
        "friendly name is not mandatory")
require('StatusLine("ID", device.deviceId)' in list_screen and
        'StatusLine("Адреса", device.baseUrl.ifBlank { "—" })' in list_screen,
        "technical ID/address missing from Properties")
require("Text(device.baseUrl" not in list_screen,
        "raw endpoint leaked into normal device card")

# One physical ESP must collapse to one owner-visible record. Persistence and
# visible LAN/CLOUD state must use the same canonical identity algorithm.
for needle in (
    "DeviceIdentity.samePhysicalDevice(",
    "current.removeAll { sameDevice(it, device) }",
    "current.removeAll { sameDevice(it, discovered) }",
    "deduplicate(loaded)",
):
    require(needle in store, f"duplicate merge contract missing: {needle}")
require("private fun newestDiscoveryFor(" in list_screen and
        "DeviceIdentity.samePhysicalDevice(" in list_screen,
        "device cards/properties do not use canonical ESP identity")
require("candidate.deviceId.equals(device.deviceId" not in list_screen,
        "device list still uses a second simplified identity matcher")

# Small icon + visible LAN/CLOUD state picons are required on the card.
require("Modifier.size(36.dp)" in list_screen,
        "device card icon is not the approved compact size")
require('"● LAN"' in list_screen and '"○ LAN"' in list_screen,
        "LAN state picon missing")
require('"☁ CLOUD"' in list_screen,
        "Cloud state picon missing")
require('"● online"' in list_screen and '"○ offline"' in list_screen,
        "overall connection state missing")

# Controller maintenance must expose PIN reveal and explicit destructive reset.
require("var pinVisible by remember" in maintenance and
        'Text(if (pinVisible) "Сховати" else "Показати")' in maintenance,
        "maintenance PIN show/hide control missing")
require('Text("СТЕРТИ ВСЕ")' in maintenance,
        "Android Factory Reset destructive confirmation missing")
require("maintenance.factoryReset(authenticated, pin)" in maintenance,
        "Android Factory Reset is not connected to controller client")
require("registeredDevices.markAuthorization(resetDeviceId, false)" in maintenance,
        "Android leaves reset controller locally authorized")
require("settings.clearControllerSessionAfterFactoryReset()" in maintenance,
        "Android does not clear selected controller session after reset")
require("apiToken = \"\"" in settings and "telemetryToken = \"\"" in settings and
        "lastKnownLocalUrl = \"\"" in settings and "localCertificateSha256 = \"\"" in settings,
        "Factory Reset session cleanup leaves stale URL/certificate/token data")
require("path = ControlPath.OFFLINE" in maintenance,
        "Android does not force endpoint offline after reset")
require("Intent.FLAG_ACTIVITY_CLEAR_TASK" in maintenance and "Intent.FLAG_ACTIVITY_NEW_TASK" in maintenance,
        "Android may reuse stale discovery/session Activity state after reset")
require("MainActivity::class.java" in maintenance,
        "Android does not return to the device-list activity after reset")
require("/api/v1/system/factory-reset" in client and "ERASE_ALL" in client,
        "Android maintenance client does not call true Factory Reset endpoint")

if errors:
    print("Android field acceptance FAIL")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("Android field acceptance PASS")
print(" - device list primary; Add flow hidden")
print(" - mandatory friendly names; ID/IP only in Properties")
print(" - duplicate physical ESP records merged with canonical identity")
print(" - compact icon + LAN/CLOUD/online picons")
print(" - PIN reveal + explicit Factory Reset")
print(" - reset clears local authorization, endpoint, certificate and cached tokens")
print(" - reset restarts Android in a fresh task so stale discovery cannot stay online")
