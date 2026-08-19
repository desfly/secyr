from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "firmware" / "esp-idf" / "main" / "hg_cloud_http.cpp"
text = SOURCE.read_text(encoding="utf-8")

errors = []
required = [
    '"/api/v1/cloud/config"',
    "HTTP_POST",
    "request_auth::authenticated_actor(request, *access_control_, actor)",
    'access_control_->authorize(actor, credential, "cloud.configure")',
    "store_->save(config)",
    "rolledBack",
]
for snippet in required:
    if snippet not in text:
        errors.append(f"hg_cloud_http.cpp missing cloud-config contract: {snippet}")

auth_pos = text.find("request_auth::authenticated_actor(request, *access_control_, actor)")
rbac_pos = text.find('access_control_->authorize(actor, credential, "cloud.configure")')
save_pos = text.find("store_->save(config)")
if min(auth_pos, rbac_pos, save_pos) < 0 or not (auth_pos < rbac_pos < save_pos):
    errors.append("cloud config must authenticate actor and authorize command before persistence")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Cloud config boundary audit PASS")
