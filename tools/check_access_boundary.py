from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "esp-idf" / "main"
WEB = ROOT / "web"

errors = []

def require(path: Path, snippets: list[str]) -> None:
    text = path.read_text(encoding="utf-8")
    for snippet in snippets:
        if snippet not in text:
            errors.append(f"{path.relative_to(ROOT)} missing security contract: {snippet}")

# Public access lifecycle must expose only the minimal setup/login decision and
# successful login must issue an opaque HTTP session token.
require(MAIN / "hg_access_http.cpp", [
    '"/api/v1/access/state"',
    '"setup_required"',
    '"login_required"',
    'http_session::issue()',
    '\"sessionToken\"',
])

# Protected read APIs must invoke the shared request authentication gate.
for filename in (
    "hg_system_http.cpp",
    "hg_cloud_http.cpp",
    "hg_lan_http.cpp",
    "hg_infrastructure_http.cpp",
    "hg_build_http.cpp",
    "hg_service_http.cpp",
):
    require(MAIN / filename, ["hg_request_auth.hpp", "request_auth::"])

# Shared gate must accept expiring bearer sessions; PIN-per-refresh must not be
# the primary browser transport.
require(MAIN / "hg_request_auth.hpp", [
    "hg_http_session.hpp",
    "http_session::authorized",
    'WWW-Authenticate',
])
require(MAIN / "hg_http_session.hpp", [
    "kLifetimeUs",
    "BearerTokenVerifier",
    "revoke_all",
])

# Full factory reset from HTTP must never erase live mutable NVS directly.
system_http = (MAIN / "hg_system_http.cpp").read_text(encoding="utf-8")
if "stage_factory_reset_request()" not in system_http:
    errors.append("hg_system_http.cpp must stage factory reset for early boot")
if "FactoryResetManager{}.erase_mutable_state()" in system_http:
    errors.append("hg_system_http.cpp must not erase mutable state in live HTTP runtime")

# Physical reset acknowledgement semantics are safety-significant.
reset = (MAIN / "hg_reset_sequence.cpp").read_text(encoding="utf-8")
for snippet in (
    "set_white(board::kOnboardRgb)",
    "stage_factory_reset_request()",
    "set_red(board::kOnboardRgb)",
    "kRequiredHolds = 3U",
):
    if snippet not in reset:
        errors.append(f"hg_reset_sequence.cpp missing reset contract: {snippet}")

# Locked web UI must exist and use the bearer token issued by login.
require(WEB / "access-session.js", [
    "hg-auth-locked",
    "/api/v1/access/state",
    "/api/v1/access/login",
    "sessionToken",
    "Bearer ${session.token}",
])

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Access boundary security audit PASS")
