#!/usr/bin/env python3
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
controller=(ROOT/"android/app/src/main/java/ua/homeguard/s3/control/CommandController.kt").read_text()
http=(ROOT/"android/app/src/main/java/ua/homeguard/s3/network/HttpDeviceApi.kt").read_text()
assert 'cloud_lock_unsupported' not in controller
assert 'cloudSemanticCommand("output.lock")' in controller
assert 'target.path == ControlPath.CLOUD' in controller
assert 'CloudAccountAuth.signedIn()' in controller
assert 'CloudAccountAuth.idToken()' in controller
assert 'appSettings.apiToken.isBlank()' not in controller
assert 'suspend fun cloudSemanticCommand(command: String)' in http
assert 'LegacyApiContract.COMMAND_PATH' in http
assert 'JSONObject().put("command", command)' in http
assert 'tokenProvider().isBlank()' in http
print("Android cloud lock contract PASS")
