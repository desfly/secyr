"""Authenticated mobile push-token registration boundary."""
from __future__ import annotations
from .binding_store import BindingStore
from .http_endpoint import HttpResult, TokenVerifier
from .push_store import PushTokenStore

class PushTokenEndpoint:
    def __init__(self,tokens:TokenVerifier,bindings:BindingStore,push_tokens:PushTokenStore):
        self.tokens=tokens; self.bindings=bindings; self.push_tokens=push_tokens

    def post(self,*,bearer_token:str,device_id:str,body:dict)->HttpResult:
        actor=self.tokens.actor(bearer_token)
        if not actor: return HttpResult(401,{"ok":False,"reason":"unauthorized"})
        if not self.bindings.authorized(actor,device_id):
            return HttpResult(404,{"ok":False,"reason":"device_not_found"})
        token=str(body.get("token","")).strip()
        if not token or len(token)>4096:
            return HttpResult(400,{"ok":False,"reason":"invalid_push_token"})
        self.push_tokens.set(actor=actor,device_id=device_id,token=token,active=True)
        return HttpResult(204,{})

    def delete(self,*,bearer_token:str,device_id:str,body:dict)->HttpResult:
        actor=self.tokens.actor(bearer_token)
        if not actor: return HttpResult(401,{"ok":False,"reason":"unauthorized"})
        if not self.bindings.authorized(actor,device_id):
            return HttpResult(404,{"ok":False,"reason":"device_not_found"})
        token=str(body.get("token","")).strip()
        if not token: return HttpResult(400,{"ok":False,"reason":"invalid_push_token"})
        self.push_tokens.revoke_for(actor=actor,device_id=device_id,token=token)
        return HttpResult(204,{})
