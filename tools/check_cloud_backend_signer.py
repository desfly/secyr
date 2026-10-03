#!/usr/bin/env python3
import importlib.util, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
pkg=ROOT/"cloud-backend"
import sys; sys.path.insert(0,str(pkg))
from homeguard_cloud.envelope import unsigned_envelope
from homeguard_cloud.signer import sign
with tempfile.TemporaryDirectory() as d:
    key=Path(d)/"key.pem"; pub=Path(d)/"pub.pem"; data=Path(d)/"data"; sig=Path(d)/"sig.der"
    subprocess.run(["openssl","ecparam","-name","prime256v1","-genkey","-noout","-out",str(key)],check=True)
    subprocess.run(["openssl","pkey","-in",str(key),"-pubout","-out",str(pub)],check=True)
    e=unsigned_envelope(device_id="HG-TEST",actor="user-1",command="output.lock",key_epoch=1,counter=1,now_ms=1000,ttl_ms=5000)
    signed=sign(e,key.read_text())
    data.write_bytes(e.canonical()); sig.write_bytes(bytes.fromhex(signed.signature))
    subprocess.run(["openssl","dgst","-sha256","-verify",str(pub),"-signature",str(sig),str(data)],check=True)
    assert signed.payload()["command"]=="output.lock"
print("Cloud backend signer PASS")
