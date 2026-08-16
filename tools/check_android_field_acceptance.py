#!/usr/bin/env python3
"""Static field gate for the cemented Android acceptance rules."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
ANDROID = ROOT / "android/app/src/main/java/ua/homeguard/s3"
main_activity = (ANDROID / "MainActivity.kt").read_text(encoding="utf-8")
list_screen = (ANDROID / "ui/screens/DeviceListScreen.kt").read_text(encoding="utf-8")
dashboard_screen = (ANDROID / "ui/screens/DashboardScreen.kt").read_text(encoding="utf-8")
add_screen = (ANDROID / "ui/screens/AddDeviceScreen.kt").read_text(encoding="utf-8")
store = (ANDROID / "storage/RegisteredDeviceStore.kt").read_text(encoding="utf-8")
settings = (ANDROID / "storage/SettingsStore.kt").read_text(encoding="utf-8")
navigation = (ANDROID / "navigation/AppNavigation.kt").read_text(encoding="utf-8")
maintenance = (ANDROID / "ControllerMaintenanceActivity.kt").read_text(encoding="utf-8")
client = (ANDROID / "diagnostics/DeviceConfigMaintenanceClient.kt").read_text(encoding="utf-8")
access_models = (ANDROID / "model/AccessModels.kt").read_text(encoding="utf-8")
http_api = (ANDROID / "network/HttpDeviceApi.kt").read_text(encoding="utf-8")
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
require("onAddDevice:" not in list_screen and "onAddDevice:" not in dashboard_screen,
        "visible screen still exposes an Add Device callback")
require("onAddDevice =" not in main_activity and "navigation.showAddDevice()" not in main_activity,
        "MainActivity still wires a visible Add Device entry point")
require("currentScreen == AppScreen.ADD_DEVICE -> AddDeviceScreen(" in main_activity,
        "hidden Add flow was deleted instead of being retained for later redesign")

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

# No active UI control may route to a runtime command that is known to be
# unimplemented. Admin authorization is broad, but UI enablement must still
# respect runtime capabilities.
require("AccessRole.ADMIN -> capabilities.allowsOperatorCommand(command)" in access_models,
        "Admin UI bypasses runtime capabilities and can enable dead commands")
require("CommandType.SILENCE, CommandType.RESET_ALARM" in access_models and
        "CommandType.ENTER_MAINTENANCE, CommandType.EXIT_MAINTENANCE -> false" in access_models,
        "unsupported operator commands are not explicitly disabled")
require('else -> CommandReply(false, code = "runtime_command_not_wired")' in http_api,
        "runtime API fallback for unsupported commands was removed")
require("CommandType.ARM_HOME -> runtimeSecurityCommand" in http_api and
        "CommandType.OPEN_VALVES -> runtimeValveCommand" in http_api,
        "supported runtime command wiring is missing")

# Controller maintenance must expose PIN reveal and explicit destructive reset.
require("var pinVisible by remember" in maintenance and
        'Text(if (pinVisible) "Сховати" else "Показати")' in maintenance,
        "maintenance PIN show/hide control missing")
require('Text("СТЕРТИ ВСЕ")' in maintenance,
        "Android Factory Reset destructive confirmation missing")
require("maintenance.factoryReset(authenticated, pin)" in maintenance,
        "Android Factory Reset is not connected to controller client")
require("val resetBaseUrl = endpoint.value.apiBaseUrl" in maintenance and
        "registeredDevices.markAuthorization(resetDeviceId, resetBaseUrl, false)" in maintenance,
        "Android does not invalidate reset controller by physical ID/endpoint")
require("suspend fun markAuthorization(deviceId: String, baseUrl: String = \"\", authorized: Boolean)" in store and
        "DeviceIdentity.samePhysicalDevice(" in store,
        "authorization invalidation is not canonical ESP identity aware")
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
require('optBoolean("rebooting", false)' in client,
        "Android treats reset as success without controller reboot acknowledgement")

if errors:
    print("Android field acceptance FAIL")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("Android field acceptance PASS")
print(" - device list primary; Add flow hidden with no visible callbacks")
print(" - mandatory friendly names; ID/IP only in Properties")
print(" - duplicate physical ESP records merged with canonical identity")
print(" - compact icon + LAN/CLOUD/online picons")
print(" - unsupported runtime controls stay disabled even for Admin")
print(" - PIN reveal + explicit Factory Reset")
print(" - reset requires reboot acknowledgement and canonical authorization invalidation")
print(" - reset clears endpoint/certificate/tokens and restarts a fresh Android task")
