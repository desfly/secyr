#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
FILES = {
    "main": ROOT / "android/app/src/main/java/ua/homeguard/s3/MainActivity.kt",
    "dashboard": ROOT / "android/app/src/main/java/ua/homeguard/s3/ui/screens/DashboardScreen.kt",
    "cloud": ROOT / "android/app/src/main/java/ua/homeguard/s3/network/CloudStateMqttClient.kt",
    "profile": ROOT / "android/app/src/main/java/ua/homeguard/s3/security/CloudSessionProfile.kt",
    "model": ROOT / "android/app/src/main/java/ua/homeguard/s3/model/SystemModels.kt",
}

REQUIRED = {
    "main": (
        "delay(5_000L)",
        "cloudState.adminUsers.collectAsState()",
        "onLogout = ::logoutCloudProfile",
        "onUpsertAdminUser = ::upsertAdminUser",
    ),
    "dashboard": (
        "CloudAccessRole.ADMIN",
        "CloudAccessRole.USER",
        "CloudAccessRole.GUEST",
        "StatusRow(\"Wi‑Fi\", wifiStatus)",
        "StatusRow(\"Мережа\", wifiSsid)",
        "Admin → Користувачі",
        "access.users.disable",
        "access.users.enable",
        "access.users.delete",
    ),
    "cloud": (
        "refreshGuestSensors",
        "refreshAdminUsers",
        "upsertAdminUser",
        "adminUserAction",
        "CloudAuthProof.adminPayloadProof",
        "CloudAuthProof.encryptAdminPin",
    ),
    "profile": (
        "canSubscribeFullState",
        "sensorOnly",
        "parsedRole == CloudAccessRole.ADMIN",
    ),
    "model": (
        "wifiStatus",
        "wifiSsid",
    ),
}


def fail(message: str) -> None:
    print(f"android-cloud-ui gate: FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    for name, path in FILES.items():
        if not path.is_file():
            fail(f"missing {path.relative_to(ROOT)}")
        text = path.read_text(encoding="utf-8")
        for marker in REQUIRED[name]:
            if marker not in text:
                fail(f"{name}: missing marker {marker}")
    print("android-cloud-ui gate: PASS")


if __name__ == "__main__":
    main()
