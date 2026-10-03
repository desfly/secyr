"""Minimal HTTP routing boundary for HomeGuard cloud API.

The network server is deliberately separate: this router parses method/path,
Bearer auth and JSON, then delegates to authenticated domain endpoints.
"""
from __future__ import annotations
import json
import re
from .http_endpoint import HttpResult

_ROUTE=re.compile(r"^/v1/devices/([^/]+)/(claim|push-tokens|api/command)$")

class CloudApiRouter:
    def __init__(self,*,claim_endpoint,push_endpoint,command_endpoint=None):
        self.claim_endpoint=claim_endpoint
        self.push_endpoint=push_endpoint
        self.command_endpoint=command_endpoint

    def handle(self,*,method:str,path:str,headers:dict[str,str],body:bytes)->HttpResult:
        match=_ROUTE.fullmatch(path)
        if not match:
            return HttpResult(404,{"ok":False,"reason":"not_found"})
        device_id=match.group(1).strip()
        route=match.group(2)
        if not device_id:
            return HttpResult(404,{"ok":False,"reason":"not_found"})

        auth=""
        for key,value in headers.items():
            if key.lower()=="authorization":
                auth=str(value); break
        if not auth.startswith("Bearer "):
            return HttpResult(401,{"ok":False,"reason":"unauthorized"})
        bearer=auth[7:].strip()
        if not bearer:
            return HttpResult(401,{"ok":False,"reason":"unauthorized"})

        try:
            payload=json.loads(body.decode("utf-8")) if body else {}
        except (UnicodeDecodeError,json.JSONDecodeError):
            return HttpResult(400,{"ok":False,"reason":"invalid_json"})
        if not isinstance(payload,dict):
            return HttpResult(400,{"ok":False,"reason":"invalid_json"})

        verb=method.upper()
        if route=="claim" and verb=="POST":
            return self.claim_endpoint.post(bearer_token=bearer,device_id=device_id,body=payload)
        if route=="push-tokens" and verb=="POST":
            return self.push_endpoint.post(bearer_token=bearer,device_id=device_id,body=payload)
        if route=="push-tokens" and verb=="DELETE":
            return self.push_endpoint.delete(bearer_token=bearer,device_id=device_id,body=payload)
        if route=="api/command" and verb=="POST" and self.command_endpoint is not None:
            return self.command_endpoint.post(bearer_token=bearer,device_id=device_id,body=payload)
        return HttpResult(405,{"ok":False,"reason":"method_not_allowed"})
