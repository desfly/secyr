#!/usr/bin/env python3
from pathlib import Path

root=Path(__file__).resolve().parents[1]
model=(root/"android/app/src/main/java/ua/homeguard/s3/model/ProvisioningModels.kt").read_text()
ui=(root/"android/app/src/main/java/ua/homeguard/s3/ui/screens/ProvisioningScreen.kt").read_text()
coord=(root/"android/app/src/main/java/ua/homeguard/s3/repository/ProvisioningCoordinator.kt").read_text()
push=(root/"android/app/src/main/java/ua/homeguard/s3/push/PushTokenRegistrar.kt").read_text()
controller=(root/"android/app/src/main/java/ua/homeguard/s3/control/CommandController.kt").read_text()
auth=(root/"android/app/src/main/java/ua/homeguard/s3/auth/CloudAccountAuth.kt").read_text()
gradle=(root/"android/app/build.gradle.kts").read_text()

for token in ("cloudApiUrl","cloudAccountEmail","cloudAccountPassword"):
    assert token in model, token
    assert token in ui, token
assert 'startsWith("https://")' in ui
assert 'CloudAccountAuth.signIn' in coord
assert "cloudBaseUrl = form.cloudApiUrl.trimEnd" in coord
assert "apiToken = localApiToken" in coord
assert "CloudAccountAuth.idToken()" in push
assert "settings.apiToken" not in push
assert "CloudAccountAuth.idToken()" in controller
assert "else settings.settings.value.apiToken" not in controller
assert "target.path == ControlPath.CLOUD && !CloudAccountAuth.signedIn()" in controller
assert "firebase-auth" in gradle
assert "getIdToken" in auth
print("android cloud account contract: ok")
