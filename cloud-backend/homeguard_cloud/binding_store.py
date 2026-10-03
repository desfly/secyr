"""Authorization boundary for account-to-device access.

The production identity provider validates the short-lived Bearer token and
returns its actor. This store is the authoritative revocable device binding.
"""
from __future__ import annotations
import sqlite3
from pathlib import Path

class BindingStore:
    def __init__(self,path: str|Path):
        self.path=str(path)
        with sqlite3.connect(self.path) as db:
            db.execute("CREATE TABLE IF NOT EXISTS device_binding(actor TEXT NOT NULL, device_id TEXT NOT NULL, active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1)), PRIMARY KEY(actor,device_id))")
    def set(self,actor:str,device_id:str,active:bool)->None:
        if not actor or not device_id: raise ValueError("actor and device_id are required")
        with sqlite3.connect(self.path) as db:
            db.execute("INSERT INTO device_binding(actor,device_id,active) VALUES(?,?,?) ON CONFLICT(actor,device_id) DO UPDATE SET active=excluded.active",(actor,device_id,1 if active else 0)); db.commit()
    def authorized(self,actor:str,device_id:str)->bool:
        if not actor or not device_id: return False
        with sqlite3.connect(self.path) as db:
            row=db.execute("SELECT active FROM device_binding WHERE actor=? AND device_id=?",(actor,device_id)).fetchone()
        return bool(row and row[0]==1)
