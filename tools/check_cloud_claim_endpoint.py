from __future__ import annotations
import tempfile
import time
from pathlib import Path
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"cloud-backend"))

from homeguard_cloud.binding_store import BindingStore
from homeguard_cloud.claim_store import ClaimEndpoint, ClaimStore

class Tokens:
    def actor(self, token):
        return "owner" if token == "good" else None

with tempfile.TemporaryDirectory() as directory:
    db=Path(directory)/"cloud.db"
    claims=ClaimStore(db)
    bindings=BindingStore(db)
    endpoint=ClaimEndpoint(tokens=Tokens(),claims=claims,bindings=bindings)

    token="A"*32
    claims.issue(device_id="HG-1",token=token,expires_at=int(time.time())+60)

    assert endpoint.post(bearer_token="bad",device_id="HG-1",body={"claimToken":token}).status==401
    assert not bindings.authorized("owner","HG-1")
    assert endpoint.post(bearer_token="good",device_id="HG-1",body={"claimToken":"B"*32}).status==404
    assert not bindings.authorized("owner","HG-1")

    assert endpoint.post(bearer_token="good",device_id="HG-1",body={"claimToken":token}).status==204
    assert bindings.authorized("owner","HG-1")
    assert endpoint.post(bearer_token="good",device_id="HG-1",body={"claimToken":token}).status==404

    expired="C"*32
    claims.issue(device_id="HG-2",token=expired,expires_at=int(time.time())+60)
    assert not claims.consume(device_id="HG-2",token=expired,now=int(time.time())+61)
    assert not bindings.authorized("owner","HG-2")

    assert endpoint.post(bearer_token="good",device_id="HG-3",body={}).status==400

print("cloud claim endpoint: ok")
