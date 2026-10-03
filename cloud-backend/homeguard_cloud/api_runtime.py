"""Fail-closed cloud API composition.

The identity provider is injected. This module never treats a controller/local
API token as a cloud account credential.
"""
from __future__ import annotations
from pathlib import Path
from .api_router import CloudApiRouter
from .binding_store import BindingStore
from .claim_store import ClaimEndpoint, ClaimStore
from .command_service import CommandService
from .counter_store import CounterStore
from .http_endpoint import CommandEndpoint
from .push_endpoint import PushTokenEndpoint
from .push_store import PushTokenStore

def build_cloud_api(*,token_verifier,db_path:str|Path,command_publisher=None,key_epoch:int=1)->CloudApiRouter:
    bindings=BindingStore(db_path)
    claims=ClaimStore(db_path)
    push_tokens=PushTokenStore(db_path)
    command_endpoint=None
    if command_publisher is not None:
        commands=CommandService(CounterStore(db_path),command_publisher,key_epoch)
        command_endpoint=CommandEndpoint(token_verifier,bindings,commands)
    return CloudApiRouter(
        claim_endpoint=ClaimEndpoint(tokens=token_verifier,claims=claims,bindings=bindings),
        push_endpoint=PushTokenEndpoint(tokens=token_verifier,bindings=bindings,push_tokens=push_tokens),
        command_endpoint=command_endpoint,
    )
