from pathlib import Path
import tempfile
from homeguard_cloud.push_store import PushTokenStore
from homeguard_cloud.event_push import EventPushService

class Push:
    def __init__(self): self.items=[]
    def send(self,**kwargs): self.items.append(kwargs)

def main():
    with tempfile.TemporaryDirectory() as td:
        store=PushTokenStore(Path(td)/"push.db")
        store.set(actor="u1",device_id="HG-1",token="phone-a")
        store.set(actor="u2",device_id="HG-1",token="phone-b")
        store.set(actor="u3",device_id="HG-2",token="other")
        push=Push(); svc=EventPushService(store,push)
        assert svc.handle(device_id="HG-1",payload=b'{"event":"alarm","sourceId":2,"seq":7}')==2
        assert {x["token"] for x in push.items}=={"phone-a","phone-b"}
        assert all(x["high_priority"] for x in push.items)
        assert all(x["data"]["type"]=="HOMEGUARD_ALARM" for x in push.items)
        assert svc.handle(device_id="HG-1",payload=b'{"event":"armed"}')==0
        store.revoke("phone-a"); push.items.clear()
        assert svc.handle(device_id="HG-1",payload=b'{"event":"tamper"}')==1
        assert push.items[0]["token"]=="phone-b"
    print("HomeGuard event push contract: OK")

if __name__=="__main__": main()
