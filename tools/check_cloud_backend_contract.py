#!/usr/bin/env python3
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"cloud-backend"))
from homeguard_cloud.envelope import unsigned_envelope
e=unsigned_envelope(device_id="HG-TEST",actor="user-1",command="output.lock",key_epoch=7,counter=42,now_ms=1000,ttl_ms=5000)
expected=(
"version=1\n"
"deviceId=HG-TEST\n"
f"requestId={e.requestId}\n"
"actor=user-1\n"
"command=output.lock\n"
"keyEpoch=7\n"
"counter=42\n"
"issuedAtMs=1000\n"
"expiresAtMs=6000\n"
"challenge="
).encode()
assert e.canonical()==expected
p=e.payload()
assert p["command"]=="output.lock" and p["challenge"]=="" and p["signature"]==""
firmware=(ROOT/"firmware/esp-idf/main/hg_cloud_link.cpp").read_text()
assert 'if (command == "output.lock")' in firmware
assert '{5, true, false, 0}' in firmware
contract=(ROOT/"docs/CLOUD-BACKEND-CONTRACT.md").read_text()
for key in ("version","deviceId","requestId","actor","command","keyEpoch","counter","issuedAtMs","expiresAtMs","challenge","signature"):
    assert f'"{key}"' in contract
print("Cloud backend envelope contract PASS")
