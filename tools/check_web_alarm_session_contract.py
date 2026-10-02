#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "web" / "access-session.js").read_text(encoding="utf-8")

assert 'response.status === 401 && session' in source
assert 'originalFetch("/api/v1/access/users"' in source
assert '"Authorization":authHeader()' in source
assert 'body:JSON.stringify({action:"list",actor:session.actor})' in source
assert 'probeResponse.status === 401 && session' in source

# The public lifecycle endpoint always reports login_required after setup; it
# must never be used as proof that an existing bearer token was revoked.
recovery = source[source.index('if (response.status === 401 && session)'):source.index('function syncActorFields')]
assert 'originalFetch("/api/v1/access/state"' not in recovery
print("web alarm/session recovery contract: ok")
