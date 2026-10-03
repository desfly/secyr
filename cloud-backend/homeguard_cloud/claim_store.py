"""Single-use cloud claim tokens for binding an account to a HomeGuard device."""
from __future__ import annotations
import hashlib
import hmac
import sqlite3
import time
from pathlib import Path
from .binding_store import BindingStore
from .http_endpoint import HttpResult, TokenVerifier

class ClaimStore:
    def __init__(self,path: str|Path):
        self.path=str(path)
        with sqlite3.connect(self.path) as db:
            db.execute("""CREATE TABLE IF NOT EXISTS device_claim(
                device_id TEXT PRIMARY KEY,
                token_sha256 TEXT NOT NULL,
                expires_at INTEGER NOT NULL,
                consumed INTEGER NOT NULL DEFAULT 0 CHECK(consumed IN (0,1))
            )""")

    def issue(self,*,device_id:str,token:str,expires_at:int)->None:
        device_id=device_id.strip(); token=token.strip()
        if not device_id or len(token)<32 or expires_at<=int(time.time()):
            raise ValueError("invalid_claim")
        digest=hashlib.sha256(token.encode("utf-8")).hexdigest()
        with sqlite3.connect(self.path) as db:
            db.execute(
                "INSERT INTO device_claim(device_id,token_sha256,expires_at,consumed) VALUES(?,?,?,0) "
                "ON CONFLICT(device_id) DO UPDATE SET token_sha256=excluded.token_sha256,expires_at=excluded.expires_at,consumed=0",
                (device_id,digest,expires_at),
            )
            db.commit()

    def consume(self,*,device_id:str,token:str,now:int|None=None)->bool:
        device_id=device_id.strip(); token=token.strip()
        if not device_id or not token: return False
        current=int(time.time()) if now is None else int(now)
        digest=hashlib.sha256(token.encode("utf-8")).hexdigest()
        with sqlite3.connect(self.path) as db:
            db.execute("BEGIN IMMEDIATE")
            row=db.execute("SELECT token_sha256,expires_at,consumed FROM device_claim WHERE device_id=?",(device_id,)).fetchone()
            if not row or row[2] or current>int(row[1]) or not hmac.compare_digest(str(row[0]),digest):
                db.rollback(); return False
            changed=db.execute("UPDATE device_claim SET consumed=1 WHERE device_id=? AND consumed=0",(device_id,)).rowcount
            db.commit()
        return changed==1

class ClaimEndpoint:
    def __init__(self,*,tokens:TokenVerifier,claims:ClaimStore,bindings:BindingStore):
        self.tokens=tokens; self.claims=claims; self.bindings=bindings

    def post(self,*,bearer_token:str,device_id:str,body:dict)->HttpResult:
        actor=self.tokens.actor(bearer_token)
        if not actor: return HttpResult(401,{"ok":False,"reason":"unauthorized"})
        claim=str(body.get("claimToken","")).strip()
        if not claim or len(claim)>256: return HttpResult(400,{"ok":False,"reason":"invalid_claim_token"})
        if not self.claims.consume(device_id=device_id,token=claim): return HttpResult(404,{"ok":False,"reason":"claim_not_found"})
        self.bindings.set(actor,device_id,True)
        return HttpResult(204,{})
