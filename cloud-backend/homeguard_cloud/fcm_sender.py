"""Firebase Cloud Messaging HTTP v1 provider adapter.

OAuth access-token acquisition is deliberately injected. Production can use
Google Application Default Credentials without storing a service-account key
in this repository.
"""
from __future__ import annotations
import json
from typing import Callable
from urllib import request

class FcmHttpV1Sender:
    def __init__(self,*,project_id:str,access_token:Callable[[],str],opener:Callable[...,object]=request.urlopen):
        project_id=project_id.strip()
        if not project_id: raise ValueError("project_id_required")
        self.project_id=project_id
        self.access_token=access_token
        self.opener=opener

    @property
    def endpoint(self)->str:
        return f"https://fcm.googleapis.com/v1/projects/{self.project_id}/messages:send"

    def send(self,*,token:str,data:dict[str,str],high_priority:bool)->None:
        token=token.strip()
        if not token: raise ValueError("fcm_token_required")
        bearer=self.access_token().strip()
        if not bearer: raise RuntimeError("fcm_access_token_required")
        payload={"message":{"token":token,"data":{str(k):str(v) for k,v in data.items()},"android":{"priority":"HIGH" if high_priority else "NORMAL"}}}
        req=request.Request(
            self.endpoint,
            data=json.dumps(payload,separators=(",",":")).encode("utf-8"),
            headers={"Authorization":f"Bearer {bearer}","Content-Type":"application/json; charset=utf-8","Accept":"application/json"},
            method="POST",
        )
        with self.opener(req,timeout=10) as response:
            status=getattr(response,"status",200)
            if not 200 <= status < 300: raise RuntimeError(f"fcm_http_{status}")
