from __future__ import annotations
import tempfile
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"cloud-backend"))

from homeguard_cloud.api_runtime import build_cloud_api
from homeguard_cloud.claim_store import ClaimStore
from homeguard_cloud.push_store import PushTokenStore
import os

class Tokens:
    def actor(self,token):
        return "owner" if token=="account-access" else None

with tempfile.TemporaryDirectory() as directory:
    db=Path(directory)/"homeguard.db"
    claims=ClaimStore(db)
    claim="Q"*32
    import time
    claims.issue(device_id="HG-1",token=claim,expires_at=int(time.time())+60)

    published=[]
    class Publisher:
        def publish(self,topic,payload,qos): published.append((topic,payload,qos))
    old_key=os.environ.get("HOMEGUARD_CLOUD_COMMAND_PRIVATE_KEY_PEM")
    os.environ["HOMEGUARD_CLOUD_COMMAND_PRIVATE_KEY_PEM"]="""-----BEGIN PRIVATE KEY-----\nMC4CAQAwBQYDK2VwBCIEIKAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n-----END PRIVATE KEY-----"""
    api=build_cloud_api(token_verifier=Tokens(),db_path=db,command_publisher=Publisher())
    auth={"Authorization":"Bearer account-access"}

    result=api.handle(method="POST",path="/v1/devices/HG-1/claim",headers=auth,body=("{\"claimToken\":\""+claim+"\"}").encode())
    assert result.status==204

    result=api.handle(method="POST",path="/v1/devices/HG-1/push-tokens",headers=auth,body=b'{"token":"fcm-1"}')
    assert result.status==204
    assert PushTokenStore(db).active_tokens("HG-1")==["fcm-1"]

    result=api.handle(method="POST",path="/v1/devices/HG-1/push-tokens",headers={"Authorization":"Bearer local-controller-token"},body=b'{"token":"bad"}')
    assert result.status==401
    assert PushTokenStore(db).active_tokens("HG-1")==["fcm-1"]

    # Router/runtime must expose the command endpoint when a publisher is composed.
    assert api.command_endpoint is not None
    if old_key is None: os.environ.pop("HOMEGUARD_CLOUD_COMMAND_PRIVATE_KEY_PEM",None)
    else: os.environ["HOMEGUARD_CLOUD_COMMAND_PRIVATE_KEY_PEM"]=old_key

print("cloud api runtime: ok")
