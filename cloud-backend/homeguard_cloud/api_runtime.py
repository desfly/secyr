"""Fail-closed cloud API composition.

The identity provider is injected. This module never treats a controller/local
API token as a cloud account credential.
"""
from __future__ import annotations
from pathlib import Path
from .api_router import CloudApiRouter
from .binding_store import BindingStore
from .claim_store import ClaimEndpoint, ClaimStore
from .push_endpoint import PushTokenEndpoint
from .push_store import PushTokenStore

def build_cloud_api(*,token_verifier,db_path:str|Path)->CloudApiRouter:
    bindings=BindingStore(db_path)
    claims=ClaimStore(db_path)
    push_tokens=PushTokenStore(db_path)
    return CloudApiRouter(
        claim_endpoint=ClaimEndpoint(tokens=token_verifier,claims=claims,bindings=bindings),
        push_endpoint=PushTokenEndpoint(tokens=token_verifier,bindings=bindings,push_tokens=push_tokens),
    )
