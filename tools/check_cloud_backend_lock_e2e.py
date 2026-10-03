#!/usr/bin/env python3
import os,sys,tempfile,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(ROOT/"cloud-backend"))
from homeguard_cloud.binding_store import BindingStore
from homeguard_cloud.counter_store import CounterStore
from homeguard_cloud.command_service import CommandService
from homeguard_cloud.http_endpoint import CommandEndpoint

class Tokens:
    def actor(self,t): return "user-1" if t=="good" else None
class Pub:
    def __init__(self): self.calls=[]
    def publish(self,topic,payload,qos): self.calls.append((topic,payload,qos))

with tempfile.TemporaryDirectory() as d:
    key=Path(d)/"key.pem"
    subprocess.run(["openssl","ecparam","-name","prime256v1","-genkey","-noout","-out",str(key)],check=True)
    os.environ["HOMEGUARD_CLOUD_COMMAND_PRIVATE_KEY_PEM"]=key.read_text()
    bindings=BindingStore(Path(d)/"bindings.db"); counters=CounterStore(Path(d)/"counter.db"); pub=Pub()
    ep=CommandEndpoint(Tokens(),bindings,CommandService(counters,pub,1))
    assert ep.post(bearer_token="bad",device_id="HG-A",body={"command":"output.lock"}).status==401
    assert ep.post(bearer_token="good",device_id="HG-A",body={"command":"output.lock"}).status==404
    assert pub.calls==[]
    bindings.set("user-1","HG-A",True)
    ok=ep.post(bearer_token="good",device_id="HG-A",body={"command":"output.lock"})
    assert ok.status==202 and ok.body["ok"] and len(pub.calls)==1
    assert pub.calls[0][0]=="homeguard/v1/devices/HG-A/commands" and pub.calls[0][2]==1
    bindings.set("user-1","HG-A",False)
    assert ep.post(bearer_token="good",device_id="HG-A",body={"command":"output.lock"}).status==404
    assert len(pub.calls)==1
print("Cloud backend output.lock end-to-end PASS")
