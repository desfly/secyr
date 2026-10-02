#!/usr/bin/env python3
import tempfile
from pathlib import Path
from homeguard_cloud.binding_store import BindingStore
from homeguard_cloud.push_store import PushTokenStore
from homeguard_cloud.push_endpoint import PushTokenEndpoint

class Tokens:
    def actor(self, token):
        return "owner" if token == "good" else None

with tempfile.TemporaryDirectory() as td:
    bindings=BindingStore(Path(td)/"bindings.db")
    push=PushTokenStore(Path(td)/"push.db")
    bindings.set(actor="owner",device_id="HG-1",active=True)
    endpoint=PushTokenEndpoint(Tokens(),bindings,push)

    assert endpoint.post(bearer_token="bad",device_id="HG-1",body={"token":"fcm-a"}).status == 401
    assert endpoint.post(bearer_token="good",device_id="HG-2",body={"token":"fcm-a"}).status == 404
    assert endpoint.post(bearer_token="good",device_id="HG-1",body={"token":""}).status == 400
    assert endpoint.post(bearer_token="good",device_id="HG-1",body={"token":"fcm-a"}).status == 204
    assert push.active_tokens("HG-1") == ["fcm-a"]
    assert endpoint.delete(bearer_token="good",device_id="HG-1",body={"token":"fcm-a"}).status == 204
    assert push.active_tokens("HG-1") == []
print("cloud push token endpoint: OK")
