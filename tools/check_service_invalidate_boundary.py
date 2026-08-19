from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "firmware" / "esp-idf" / "main" / "hg_service_http.cpp"
text = SOURCE.read_text(encoding="utf-8")

errors = []
required = [
    '"/api/v1/service/invalidate"',
    "HTTP_POST",
    "request_auth::authenticated_actor(request, *self->access_control_, actor)",
    'self->access_control_->authorize_session(actor, "system.service.invalidate")',
    "self->store_->erase_all()",
]
for snippet in required:
    if snippet not in text:
        errors.append(f"hg_service_http.cpp missing service-invalidate contract: {snippet}")

auth_pos = text.find("request_auth::authenticated_actor(request, *self->access_control_, actor)")
rbac_pos = text.find('self->access_control_->authorize_session(actor, "system.service.invalidate")')
erase_pos = text.find("self->store_->erase_all()")
if min(auth_pos, rbac_pos, erase_pos) < 0 or not (auth_pos < rbac_pos < erase_pos):
    errors.append("service invalidation must authenticate Bearer actor and authorize session role before erase_all")

if 'self->access_control_->authorize(actor, credential, "system.service.invalidate")' in text:
    errors.append("service invalidation must not re-check acting Admin PIN after Bearer login")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Service invalidate boundary audit PASS")
