"""Revocable Android push-token registry bound to authorized accounts/devices."""
from __future__ import annotations
import sqlite3
from pathlib import Path

class PushTokenStore:
    def __init__(self,path: str|Path):
        self.path=str(path)
        with sqlite3.connect(self.path) as db:
            db.execute("""CREATE TABLE IF NOT EXISTS push_token(
                actor TEXT NOT NULL,
                device_id TEXT NOT NULL,
                token TEXT NOT NULL,
                active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1)),
                PRIMARY KEY(actor,device_id,token)
            )""")
    def set(self,*,actor:str,device_id:str,token:str,active:bool=True)->None:
        if not actor or not device_id or not token: raise ValueError("actor, device_id and token are required")
        with sqlite3.connect(self.path) as db:
            db.execute("INSERT INTO push_token(actor,device_id,token,active) VALUES(?,?,?,?) ON CONFLICT(actor,device_id,token) DO UPDATE SET active=excluded.active",(actor,device_id,token,1 if active else 0))
            db.commit()
    def active_tokens(self,device_id:str)->list[str]:
        if not device_id: return []
        with sqlite3.connect(self.path) as db:
            rows=db.execute("SELECT DISTINCT token FROM push_token WHERE device_id=? AND active=1",(device_id,)).fetchall()
        return [str(row[0]) for row in rows]
    def revoke(self,token:str)->None:
        with sqlite3.connect(self.path) as db:
            db.execute("UPDATE push_token SET active=0 WHERE token=?",(token,))
            db.commit()
