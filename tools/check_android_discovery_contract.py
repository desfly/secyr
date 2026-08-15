#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
udp = (ROOT / "android/app/src/main/java/ua/homeguard/s3/network/UdpDeviceDiscovery.kt").read_text(encoding="utf-8")
nsd = (ROOT / "android/app/src/main/java/ua/homeguard/s3/network/NsdDeviceDiscovery.kt").read_text(encoding="utf-8")
http = (ROOT / "android/app/src/main/java/ua/homeguard/s3/network/HttpSubnetDiscovery.kt").read_text(encoding="utf-8")
coordinator = (ROOT / "android/app/src/main/java/ua/homeguard/s3/network/LocalDiscoveryCoordinator.kt").read_text(encoding="utf-8")
models = (ROOT / "android/app/src/main/java/ua/homeguard/s3/model/ConnectivityModels.kt").read_text(encoding="utf-8")

checks = {
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
    "HTTP HomeGuard identity endpoint": '"/api/v1/cloud/status"' in http,
    "HTTP discovery source model": "enum class DiscoverySource { MDNS, UDP, HTTP }" in models,
    "Coordinator combines mDNS+UDP+HTTP": "combine(nsd.devices, udp.devices, http.devices)" in coordinator,
    "Coordinator triggers HTTP fallback": "http.scanOnce()" in coordinator,
    "Coordinator exposes scan status": "val scanStatus" in coordinator and "udp.status" in coordinator,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"{'PASS' if ok else 'FAIL'}: {name}")

if failed:
    raise SystemExit("Android discovery contract failed: " + ", ".join(failed))

print(f"Android discovery contract PASS ({len(checks)} checks)")
