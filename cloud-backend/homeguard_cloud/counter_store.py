"""Durable monotonic command counters for firmware replay protection."""
from __future__ import annotations
import sqlite3
from pathlib import Path

class CounterStore:
    def __init__(self, path: str | Path):
        self.path=str(path)
        with sqlite3.connect(self.path) as db:
            db.execute("CREATE TABLE IF NOT EXISTS command_counter (device_id TEXT PRIMARY KEY, value INTEGER NOT NULL CHECK(value>=0))")

    def next(self, device_id: str) -> int:
        if not device_id: raise ValueError("device_id is required")
        with sqlite3.connect(self.path, isolation_level="IMMEDIATE") as db:
            db.execute("BEGIN IMMEDIATE")
            row=db.execute("SELECT value FROM command_counter WHERE device_id=?",(device_id,)).fetchone()
            value=(row[0] if row else 0)+1
            db.execute("INSERT INTO command_counter(device_id,value) VALUES(?,?) ON CONFLICT(device_id) DO UPDATE SET value=excluded.value",(device_id,value))
            db.commit()
            return value
