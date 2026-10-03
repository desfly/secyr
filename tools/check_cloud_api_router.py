from __future__ import annotations
import tempfile
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"cloud-backend"))

from homeguard_cloud.api_router import CloudApiRouter
from homeguard_cloud.http_endpoint import HttpResult

class Endpoint:
    def __init__(self): self.calls=[]
    def post(self,**kwargs): self.calls.append(("POST",kwargs)); return HttpResult(204,{})
    def delete(self,**kwargs): self.calls.append(("DELETE",kwargs)); return HttpResult(204,{})

claim=Endpoint(); push=Endpoint()
router=CloudApiRouter(claim_endpoint=claim,push_endpoint=push)
auth={"Authorization":"Bearer abc"}

assert router.handle(method="POST",path="/v1/devices/HG-1/claim",headers=auth,body=b'{"claimToken":"x"}').status==204
assert claim.calls[-1][1]["bearer_token"]=="abc"
assert claim.calls[-1][1]["device_id"]=="HG-1"
assert router.handle(method="POST",path="/v1/devices/HG-1/push-tokens",headers=auth,body=b'{"token":"fcm"}').status==204
assert router.handle(method="DELETE",path="/v1/devices/HG-1/push-tokens",headers=auth,body=b'{"token":"fcm"}').status==204
assert router.handle(method="POST",path="/v1/devices/HG-1/claim",headers={},body=b"{}").status==401
assert router.handle(method="POST",path="/v1/devices/HG-1/claim",headers=auth,body=b"{").status==400
assert router.handle(method="POST",path="/v1/devices/HG-1/claim",headers=auth,body=b"[]").status==400
assert router.handle(method="DELETE",path="/v1/devices/HG-1/claim",headers=auth,body=b"{}").status==405
assert router.handle(method="POST",path="/wrong",headers=auth,body=b"{}").status==404
print("cloud api router: ok")
