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
    "ACCESS_STATE_PATH", "ACCESS_USERS_PATH", "ACCESS_LOGIN_PATH", "TELEMETRY_SESSION_PATH",
])
require(JAVA / "network" / "HttpDeviceApi.kt", [
    "suspend fun accessState()", "suspend fun bootstrapAdmin(",
    "setupWifiScan", "setupConfigureWifi", "sessionToken.matches",
    "suspend fun telemetrySession(actor: String)",
    'JSONObject().put("command", command).put("actor", actor)',
    'JSONObject().put("outputId", outputId).put("active", active)',
    'header("Authorization", "Bearer $token")',
])
http_api = (JAVA / "network" / "HttpDeviceApi.kt").read_text(encoding="utf-8")
for forbidden in (
    'runtimeSecurityCommand(command: String, actor: String, credential: String)',
    'runtimeValveCommand(active: Boolean, actor: String, credential: String)',
    'telemetrySession(actor: String, credential: String)',
    '.put("actor", actor).put("credential", credential)',
):
    if forbidden in http_api:
        errors.append(f"HttpDeviceApi local runtime still uses acting PIN after login: {forbidden}")

require(JAVA / "control" / "CommandController.kt", [
    "localHttpSessionToken", "suspend fun accessState()", "fun logout()",
    "if (localRuntime) localHttpSessionToken",
    "api.telemetrySession(session.actor)",
    'credential = if (target.path == ControlPath.CLOUD) credential else ""',
])
require(JAVA / "network" / "FactoryResetClient.kt", [
    "private val sessionToken: String",
    'header("Authorization", "Bearer $sessionToken")',
    '.put("confirm", "ERASE_ALL")',
    "suspend fun reset(actor: String)",
])
factory = (JAVA / "network" / "FactoryResetClient.kt").read_text(encoding="utf-8")
if '.put("credential"' in factory:
    errors.append("FactoryResetClient must not transmit acting Admin PIN after login")

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

# v2 completion gate: once login succeeds, MainActivity must clear the PIN and
# must not require it for runtime commands/reset. This intentionally goes red
# until the UI state has been migrated; it prevents a half-v1/half-v2 APK.
if "operatorPin.value = \"\"" not in main:
    errors.append("MainActivity must have an explicit PIN clearing path")
if "PIN сеансу відсутній — увійдіть знову" in main:
    errors.append("MainActivity still requires stored PIN for post-login commands/reset")
if "commands.execute(type, actor, credential)" in main:
    errors.append("MainActivity still forwards acting PIN to runtime commands")
if ".reset(actor, credential)" in main:
    errors.append("MainActivity still forwards acting PIN to Factory Reset")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Android access lifecycle audit PASS")
print(" - PIN exists only during login/bootstrap")
print(" - local runtime uses Bearer session for commands, telemetry and reset")
