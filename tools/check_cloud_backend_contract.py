#!/usr/bin/env python3
import importlib.util
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
modpath=ROOT/"cloud-backend"/"homeguard_cloud"/"envelope.py"
spec=importlib.util.spec_from_file_location("hg_envelope",modpath)
m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
e=m.unsigned_envelope(device_id="HG-TEST",actor="user-1",command="output.lock",key_epoch=7,counter=42,now_ms=1000,ttl_ms=5000)
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
