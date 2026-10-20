#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
udp = (ROOT / "android/app/src/main/java/ua/homeguard/s3/network/UdpDeviceDiscovery.kt").read_text(encoding="utf-8")
nsd = (ROOT / "android/app/src/main/java/ua/homeguard/s3/network/NsdDeviceDiscovery.kt").read_text(encoding="utf-8")
http = (ROOT / "android/app/src/main/java/ua/homeguard/s3/network/HttpSubnetDiscovery.kt").read_text(encoding="utf-8")
coordinator = (ROOT / "android/app/src/main/java/ua/homeguard/s3/network/LocalDiscoveryCoordinator.kt").read_text(encoding="utf-8")
models = (ROOT / "android/app/src/main/java/ua/homeguard/s3/model/ConnectivityModels.kt").read_text(encoding="utf-8")
firmware_discovery = (ROOT / "firmware/esp-idf/components/device_discovery/device_discovery.cpp").read_text(encoding="utf-8")
firmware_telemetry = (ROOT / "firmware/esp-idf/main/hg_telemetry_runtime.cpp").read_text(encoding="utf-8")
firmware_app = (ROOT / "firmware/esp-idf/main/app_main.cpp").read_text(encoding="utf-8")
w5500_h = (ROOT / "firmware/esp-idf/main/hg_w5500.hpp").read_text(encoding="utf-8")
w5500_cpp = (ROOT / "firmware/esp-idf/main/hg_w5500.cpp").read_text(encoding="utf-8")

checks = {
    "UDP discovery port": "const val PORT = 45678" in udp,
    "ESP discovery uses live transport": "record.transport = transport_.load();" in firmware_discovery and "discovery_->set_transport(transport);" in firmware_telemetry,
    "ESP discovery wired to telemetry": "&g_gpio_outputs, &g_device_discovery" in firmware_app,
    "Wi-Fi transport requires IPv4": "sta_ip.ip.addr != 0" in firmware_telemetry,
    "W5500 snapshot is by value": "W5500Status status() const;" in w5500_h and "const W5500Status& status()" not in w5500_h,
    "W5500 status fields guarded": "portENTER_CRITICAL(&self->status_lock_);" in w5500_cpp and "portENTER_CRITICAL(&status_lock_);" in w5500_cpp,
    "W5500 stale IP rejected": "if (!link_up)" in w5500_cpp and "event->esp_netif != self->netif_" in w5500_cpp,
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
    "Physical duplicate merge helper": ("mergePhysicalControllers(routes)" in coordinator and "(mdns + udpFallback + httpFallback)" in coordinator),
    "Stable ID survives DHCP address change": "stableDeviceId(a)" in coordinator and "aId != null && aId == bId" in coordinator,
    "Host bridges temporary setup identity": "aHost.isNotBlank() && aHost == bHost" in coordinator,
    "Temporary setup ID excluded from stable identity": 'id.startsWith("setup-")' in coordinator,
    "Old host-primary grouping removed": ".groupBy(::physicalControllerKey)" not in coordinator,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"{'PASS' if ok else 'FAIL'}: {name}")

if failed:
    raise SystemExit("Android discovery contract failed: " + ", ".join(failed))

print(f"Android discovery contract PASS ({len(checks)} checks)")
