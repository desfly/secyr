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
maintenance_panel = (ANDROID / "ui/components/MaintenancePanel.kt").read_text(encoding="utf-8")
identity = (ANDROID / "model/DeviceIdentity.kt").read_text(encoding="utf-8")
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
require("fun isForbiddenFriendlyName(" in identity,
        "shared friendly-name validator is missing")
require('clean.equals("HomeGuard", ignoreCase = true)' in identity and
        'clean.equals("HomeGuard-S3", ignoreCase = true)' in identity,
        "shared friendly-name validator does not reject generated HomeGuard names")
require("clean.equals(deviceId.trim(), ignoreCase = true)" in identity,
        "shared friendly-name validator does not reject device ID")
require("clean.equals(endpoint, ignoreCase = true)" in identity and
        "endpointHost(endpoint)" in identity and "clean.equals(host, ignoreCase = true)" in identity,
        "shared friendly-name validator does not reject endpoint/IP")
require("val nameValid = !DeviceIdentity.isForbiddenFriendlyName(cleanName)" in add_screen,
        "Add flow does not require a safe owner-friendly name")
require("enabled = selectedNameValid" in add_screen and
        "enabled = manualAddressNameValid && manualAddress.isNotBlank()" in add_screen and
        "enabled = manualIdNameValid && manualDeviceId.isNotBlank()" in add_screen,
        "Add flow buttons do not enforce safe friendly-name validation")
require("val renameValid = !DeviceIdentity.isForbiddenFriendlyName(" in list_screen and
        "enabled = renameValid" in list_screen,
        "Rename UI can accept a generated/technical display name")
require("DeviceIdentity.isForbiddenFriendlyName(value, deviceId, baseUrl)" in store,
        "persistent registry does not use the shared friendly-name validator")
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

# Controller maintenance must be reachable from the normal dashboard and must
# expose PIN reveal plus explicit destructive reset. This guards against the
# Build-948 class of bug where backend code existed but the user had no control.
require('Text("Конфігурація контролера")' in maintenance_panel,
        "Dashboard maintenance panel does not expose controller configuration")
require("ControllerMaintenanceActivity::class.java" in maintenance_panel and
        "context.startActivity(Intent(context, ControllerMaintenanceActivity::class.java))" in maintenance_panel,
        "Controller maintenance Activity is not reachable from the visible dashboard")
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

# Acceptance item 23: an expected connection loss during destructive reset
# must never leave stale authorization/endpoint/tokens in the UI. Explicit HTTP
# rejection remains a different path and must not wipe local registration.
require("class FactoryResetTransportException" in client and
        "class FactoryResetRejectedException" in client,
        "Factory Reset transport loss is not distinguished from explicit rejection")
require("throw FactoryResetTransportException(error)" in client,
        "Factory Reset request transport loss is not surfaced distinctly")
require("throw FactoryResetRejectedException(" in client,
        "Factory Reset explicit rejection does not stay distinct from transport loss")
require("catch (error: FactoryResetTransportException)" in maintenance,
        "maintenance screen does not handle expected reset disconnect explicitly")
require("val leaveAfterReset: suspend" in maintenance and
        maintenance.count("leaveAfterReset(") >= 3,
        "confirmed reset and transport-loss reset do not share fail-closed cleanup")
require("registeredDevices.markAuthorization(resetDeviceId, resetBaseUrl, false)" in maintenance and
        "settings.clearControllerSessionAfterFactoryReset()" in maintenance and
        "path = ControlPath.OFFLINE" in maintenance,
        "reset disconnect path can leave stale authorized/online state")
require('status = "Factory Reset: ${error.message ?: "rejected"}"' in maintenance,
        "explicit reset rejection no longer preserves local state for retry")

if errors:
    print("Android field acceptance FAIL")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("Android field acceptance PASS")
print(" - device list primary; Add flow hidden with no visible callbacks")
print(" - shared friendly-name validator blocks HomeGuard/ID/IP on Add + Rename + persistence")
print(" - technical ID/IP only in Properties")
print(" - duplicate physical ESP records merged with canonical identity")
print(" - compact icon + LAN/CLOUD/online picons")
print(" - unsupported runtime controls stay disabled even for Admin")
print(" - visible dashboard path opens controller maintenance")
print(" - PIN reveal + explicit Factory Reset")
print(" - reset confirms reboot when response arrives")
print(" - reset transport loss fails closed to unauthorized/offline fresh task")
print(" - explicit reset rejection preserves local state for retry")
