#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
udp = (ROOT / "android/app/src/main/java/ua/homeguard/s3/network/UdpDeviceDiscovery.kt").read_text(encoding="utf-8")
nsd = (ROOT / "android/app/src/main/java/ua/homeguard/s3/network/NsdDeviceDiscovery.kt").read_text(encoding="utf-8")
http = (ROOT / "android/app/src/main/java/ua/homeguard/s3/network/HttpSubnetDiscovery.kt").read_text(encoding="utf-8")
coordinator = (ROOT / "android/app/src/main/java/ua/homeguard/s3/network/LocalDiscoveryCoordinator.kt").read_text(encoding="utf-8")
models = (ROOT / "android/app/src/main/java/ua/homeguard/s3/model/ConnectivityModels.kt").read_text(encoding="utf-8")
add_screen = (ROOT / "android/app/src/main/java/ua/homeguard/s3/ui/screens/AddDeviceScreen.kt").read_text(encoding="utf-8")
list_screen = (ROOT / "android/app/src/main/java/ua/homeguard/s3/ui/screens/DeviceListScreen.kt").read_text(encoding="utf-8")
dashboard_screen = (ROOT / "android/app/src/main/java/ua/homeguard/s3/ui/screens/DashboardScreen.kt").read_text(encoding="utf-8")
provisioning_screen = (ROOT / "android/app/src/main/java/ua/homeguard/s3/ui/screens/ProvisioningScreen.kt").read_text(encoding="utf-8")
bruce_brand = (ROOT / "android/app/src/main/java/ua/homeguard/s3/ui/components/BruceBrand.kt").read_text(encoding="utf-8")
main_activity = (ROOT / "android/app/src/main/java/ua/homeguard/s3/MainActivity.kt").read_text(encoding="utf-8")
store = (ROOT / "android/app/src/main/java/ua/homeguard/s3/storage/RegisteredDeviceStore.kt").read_text(encoding="utf-8")

checks = {
    # Working LAN discovery is a field-proven invariant: do not rewrite it while fixing UI.
    "UDP discovery port": "const val PORT = 45678" in udp,
    "UDP request token": 'const val REQUEST = "HG_DISCOVER_V1"' in udp,
    "UDP reply protocol": 'json.optString("protocol") != "homeguard-discovery-v1"' in udp,
    "Wi-Fi socket binding": "wifiNetwork.bindSocket(socket)" in udp,
    "Wi-Fi transport selection": "NetworkCapabilities.TRANSPORT_WIFI" in udp,
    "Wi-Fi interface broadcast": "connectivity.getLinkProperties(it)?.interfaceName" in udp,
    "UDP diagnostics counters": all(token in udp for token in ("sent: Int", "received: Int", "accepted: Int", "lastResponder: String", "network: String", "error: String")),
    "mDNS service type": 'SERVICE_TYPE = "_homeguard._tcp."' in nsd,
    "mDNS concrete IP preference": "info.host?.hostAddress" in nsd,
    "mDNS independent resolver": "private fun resolve(serviceInfo: NsdServiceInfo)" in nsd and "val listener = object : NsdManager.ResolveListener" in nsd,
    "mDNS shared resolver removed": "private val resolveListener" not in nsd,
    "HTTP subnet discovery source": "class HttpSubnetDiscovery" in http,
    "HTTP HomeGuard identity endpoint": "/api/v1/cloud/status" in http,
    "HTTP discovery source model": "enum class DiscoverySource { MDNS, UDP, HTTP }" in models,
    "Coordinator combines mDNS+UDP+HTTP": "combine(nsd.devices, udp.devices, http.devices)" in coordinator,
    "Coordinator triggers HTTP fallback": "http.scanOnce()" in coordinator,
    "Coordinator exposes scan status": "val scanStatus" in coordinator and "udp.status" in coordinator,

    # Cemented product rule: start on the device list; unfinished Add UI stays hidden.
    "Device list is primary screen": "private val deviceListOpen = MutableStateFlow(true)" in main_activity,
    "Fresh app does not force Add screen": "showAddDevice || appSettings.deviceId.isBlank()" not in main_activity and "showAddDevice -> AddDeviceScreen(" in main_activity,
    "Add action hidden from main list": 'Text("+ Додати")' not in list_screen and 'Text("+ Додати пристрій")' not in list_screen,
    "Add action hidden from full monitor": 'Text("+ Додати пристрій")' not in dashboard_screen,
    "Empty list does not expose add action": "Додавання пристроїв тимчасово сховане" in list_screen,
    "Operator ID is not forced to admin": 'private val operatorId = MutableStateFlow("")' in main_activity,
    "Bruce header stays compact": "Modifier.size(56.dp)" in bruce_brand,

    # New devices require an owner-assigned name. The temporary Add screen remains
    # structurally intact for the later redesign, but these safety rules cannot regress.
    "Device name starts empty": 'var deviceName by remember { mutableStateOf("") }' in add_screen,
    "Device name is required": 'label = { Text("Назва пристрою *") }' in add_screen and "val nameValid = cleanName.isNotBlank()" in add_screen,
    "Found device save requires name": "onUseDevice(selected, cleanName)" in add_screen and "enabled = nameValid" in add_screen,
    "Manual IP save requires name": "enabled = nameValid && manualAddress.isNotBlank()" in add_screen,
    "Manual ID save requires name": "enabled = nameValid && manualDeviceId.isNotBlank()" in add_screen,
    "Manual IP autofill preserved": "LaunchedEffect(devices)" in add_screen and "manualAddress = devices.first().host" in add_screen,
    "Store rejects unnamed/generated discovery names": "displayName.isBlank() || isGeneratedName(displayName)" in store and "device.serviceName.takeIf" not in store,
    "Manual store has no generated default name": 'suspend fun addManual(deviceId: String, baseUrl: String, name: String = "")' in store,
    "Owner name wins during reconciliation": "val named = matching.firstOrNull { !isGeneratedName(it.name) }" in store and "val previous = named ?: exact" in store,
    "Dedup preserves owner name": "!isGeneratedName(existing.name) -> existing.name" in store and "!isGeneratedName(candidate.name) -> candidate.name" in store,
    "Legacy service names are not shown as user names": "isLegacyGeneratedName" in list_screen and 'clean.equals("HomeGuard-S3", ignoreCase = true)' in list_screen and '"Без назви"' in list_screen,

    # Main cards are friendly-name/state only; technical identity belongs in Properties.
    "Device cards hide raw endpoint": "Text(device.baseUrl" not in list_screen,
    "Device cards hide technical channel strip": "LinkIndicator(" not in list_screen,
    "Properties exposes ID": 'StatusLine("ID", device.deviceId)' in list_screen,
    "Properties exposes endpoint": 'StatusLine("Адреса", device.baseUrl.ifBlank { "—" })' in list_screen,
    "Properties action exists": 'Text("Властивості")' in list_screen,
    "Rename action exists": 'Text("Перейменувати")' in list_screen,
    "Delete action exists": 'Text("Видалити зі списку"' in list_screen,

    # Password visibility must remain available both for operator login and Wi-Fi setup.
    "Operator PIN visibility toggle": "var pinVisible by remember" in dashboard_screen and 'Text(if (pinVisible) "Сховати" else "Показати")' in dashboard_screen,
    "Wi-Fi password visibility toggle": "var wifiPasswordVisible by remember" in provisioning_screen and "PasswordVisualTransformation()" in provisioning_screen and "wifiPasswordVisible = !wifiPasswordVisible" in provisioning_screen,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"{'PASS' if ok else 'FAIL'}: {name}")

if failed:
    raise SystemExit("Android discovery contract failed: " + ", ".join(failed))

print(f"Android discovery contract PASS ({len(checks)} checks)")
