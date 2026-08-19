from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ui = (ROOT / "android/app/src/main/java/ua/homeguard/s3/ui/screens/ProvisioningScreen.kt").read_text(encoding="utf-8")
coord = (ROOT / "android/app/src/main/java/ua/homeguard/s3/repository/ProvisioningCoordinator.kt").read_text(encoding="utf-8")
direct = (ROOT / "android/app/src/main/java/ua/homeguard/s3/provisioning/DirectNetworkProvisioner.kt").read_text(encoding="utf-8")
fw = (ROOT / "firmware/esp-idf/main/hg_network_http.cpp").read_text(encoding="utf-8")

checks = {
    "UI direct auth fields": 'form.actor' in ui and 'form.credential' in ui,
    "UI passes setup address": 'form.copy(setupAddress = manualAddress)' in ui,
    "UI no QR-only connect gate": 'state.qr != null && form.wifiSsid' not in ui,
    "Coordinator direct path": 'provisionDirect(form)' in coord and 'DirectNetworkProvisioner.connect' in coord,
    "Direct endpoint": '/api/v1/network/connect' in direct,
    "Direct payload actor": '.put("actor", actor.trim())' in direct,
    "Direct payload credential": '.put("credential", credential)' in direct,
    "Direct payload ssid": '.put("ssid", ssid)' in direct,
    "Direct payload password": '.put("password", password)' in direct,
    "Firmware permission contract": '"system.network.configure"' in fw,
    "Old firmware permission removed": '"network.configure"' not in fw,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(("OK   " if ok else "FAIL ") + name)
if failed:
    raise SystemExit("Provisioning contract failed: " + ", ".join(failed))
print("Provisioning contract OK")
