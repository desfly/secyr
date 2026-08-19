from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
JAVA = ROOT / "android" / "app" / "src" / "main" / "java" / "ua" / "homeguard" / "s3"
errors = []

def require(path: Path, snippets: list[str]) -> None:
    text = path.read_text(encoding="utf-8")
    for snippet in snippets:
        if snippet not in text:
            errors.append(f"{path.relative_to(ROOT)} missing Android access contract: {snippet}")

require(JAVA / "model" / "AccessModels.kt", [
    "AccessLifecycleState", "SETUP_REQUIRED", "LOGIN_REQUIRED", "sessionToken",
])
require(JAVA / "network" / "RuntimeApiContract.kt", [
    "ACCESS_STATE_PATH", "ACCESS_USERS_PATH", "ACCESS_LOGIN_PATH",
])
require(JAVA / "network" / "HttpDeviceApi.kt", [
    "suspend fun accessState()", "suspend fun bootstrapAdmin(",
    "setupWifiScan", "setupConfigureWifi", "sessionToken.matches",
])
require(JAVA / "control" / "CommandController.kt", [
    "localHttpSessionToken", "suspend fun accessState()", "fun logout()",
    "if (localRuntime) localHttpSessionToken",
])
require(JAVA / "ui" / "screens" / "AccessGateScreen.kt", [
    "R.drawable.bruce_launcher", "SETUP_REQUIRED", "LOGIN_REQUIRED",
    "Сканувати Wi-Fi", "Створити Admin і закрити setup", "Увійти",
])
require(JAVA / "MainActivity.kt", [
    "currentAccessSession == null -> AccessGateScreen(",
    "commands.logout()", "refreshAccessLifecycle", "bootstrapFirstAdmin",
    "scanSetupWifi", "connectSetupWifi", "AccessLifecycleState.UNAVAILABLE",
])

main = (JAVA / "MainActivity.kt").read_text(encoding="utf-8")
if main.find("currentAccessSession == null -> AccessGateScreen(") > main.find("else -> DashboardScreen(") >= 0:
    errors.append("MainActivity must gate Dashboard before rendering it")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Android access lifecycle audit PASS")
