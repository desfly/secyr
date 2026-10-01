"""Authenticated HTTP command boundary used by Android cloud routes."""
from __future__ import annotations
from dataclasses import dataclass
from typing import Protocol
from .binding_store import BindingStore
from .command_service import CommandService

class TokenVerifier(Protocol):
    def actor(self,bearer_token:str)->str|None: ...

@dataclass(frozen=True)
class HttpResult:
    status:int
    body:dict

class CommandEndpoint:
    def __init__(self,tokens:TokenVerifier,bindings:BindingStore,commands:CommandService):
        self.tokens=tokens; self.bindings=bindings; self.commands=commands

    def post(self,*,bearer_token:str,device_id:str,body:dict)->HttpResult:
        actor=self.tokens.actor(bearer_token)
        if not actor: return HttpResult(401,{"ok":False,"reason":"unauthorized"})
        if not self.bindings.authorized(actor,device_id):
            return HttpResult(404,{"ok":False,"reason":"device_not_found"})
        command=str(body.get("command",""))
        # Initial Android quick-control surface. Expand only with explicit
        # authorization/challenge policy for each additional command.
        if command!="output.lock":
            return HttpResult(400,{"ok":False,"reason":"unsupported_command"})
        payload=self.commands.send(device_id=device_id,actor=actor,command=command)
        return HttpResult(202,{"ok":True,"requestId":payload["requestId"]})
