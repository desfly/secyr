"""Server-side HomeGuard v1 signer. No controller or broker configuration writes."""
from __future__ import annotations

import hashlib
import hmac
import json
import re
import sqlite3
import time
import uuid
from dataclasses import dataclass
from contextlib import contextmanager
from pathlib import Path

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec

COMMANDS = frozenset({"security.arm_home", "security.arm_away", "security.disarm",
                      "security.disarm_challenge", "security.panic", "output.lock"})
FIELDS = ("version", "deviceId", "requestId", "actor", "command", "keyEpoch", "counter",
          "issuedAtMs", "expiresAtMs", "challenge")


def safe_text(value: str) -> bool:
    return isinstance(value, str) and bool(value) and all(ord(c) >= 32 and ord(c) != 127 for c in value)


def canonical(envelope: dict) -> bytes:
    return "\n".join(f"{name}={envelope[name]}" for name in FIELDS).encode("utf-8")


@dataclass(frozen=True)
class DeviceKey:
    key_epoch: int
    private_key: ec.EllipticCurvePrivateKey
    counter_floor: int  # Explicitly obtained from the deployed controller; never guessed.


@dataclass(frozen=True)
class Grant:
    token_sha256: str
    expires_at_ms: int
    device_id: str
    actor: str  # An actor/session authorized by the controller, bound server-side.
    commands: frozenset[str]


class Denied(Exception):
    pass


class Signer:
    def __init__(self, database: str, devices: dict[str, DeviceKey], grants: list[Grant], clock=None):
        self.database = database
        self.devices = dict(devices)
        self.grants = tuple(grants)
        self.clock = clock or (lambda: time.time_ns() // 1_000_000)
        for device, key in self.devices.items():
            if not safe_text(device) or not isinstance(key.private_key, ec.EllipticCurvePrivateKey):
                raise ValueError("Invalid device key")
            if not 0 < key.key_epoch <= 2**63 - 1 or not 0 <= key.counter_floor < 2**63 - 1:
                raise ValueError("Invalid epoch/counter floor")
        for grant in self.grants:
            if (not re.fullmatch(r"[0-9a-f]{64}", grant.token_sha256) or
                    grant.device_id not in self.devices or not safe_text(grant.actor) or
                    not grant.commands <= COMMANDS):
                raise ValueError("Invalid account grant")
        with self._connect() as db:
            db.execute("CREATE TABLE IF NOT EXISTS counters (device TEXT PRIMARY KEY, value INTEGER NOT NULL)")
            db.execute("CREATE TABLE IF NOT EXISTS audit (request TEXT PRIMARY KEY, device TEXT NOT NULL, "
                       "actor TEXT NOT NULL, command TEXT NOT NULL, counter INTEGER NOT NULL, issued INTEGER NOT NULL)")
            for device, key in self.devices.items():
                db.execute("INSERT INTO counters VALUES (?, ?) ON CONFLICT(device) DO UPDATE "
                           "SET value=max(value, excluded.value)", (device, key.counter_floor))

    @contextmanager
    def _connect(self):
        db = sqlite3.connect(self.database, timeout=10)
        try:
            db.execute("PRAGMA synchronous=FULL")
            with db:
                yield db
        finally:
            db.close()

    def sign(self, token: str, device_id: str, request: dict) -> dict:
        now = self.clock()
        digest = hashlib.sha256(token.encode("utf-8")).hexdigest()
        grant = next((g for g in self.grants if hmac.compare_digest(g.token_sha256, digest)
                      and g.device_id == device_id and now < g.expires_at_ms), None)
        if grant is None:
            raise Denied("Not authorized")
        if not isinstance(request, dict) or set(request) - {"command", "challenge"}:
            raise ValueError("Invalid request fields")
        command = request.get("command")
        if not isinstance(command, str) or command not in COMMANDS:
            raise ValueError("Unsupported command")
        permission = "security.disarm" if command == "security.disarm_challenge" else command
        if permission not in grant.commands:
            raise Denied("Command not authorized")
        challenge = request.get("challenge", "")
        if not isinstance(challenge, str):
            raise ValueError("Invalid challenge")
        if command == "security.disarm":
            if not re.fullmatch(r"[0-9a-fA-F]{32}", challenge):
                raise ValueError("Disarm challenge required")
        elif challenge != "":
            raise ValueError("Unexpected challenge")
        key = self.devices[device_id]
        with self._connect() as db:
            db.execute("BEGIN IMMEDIATE")
            previous = db.execute("SELECT value FROM counters WHERE device=?", (device_id,)).fetchone()[0]
            if previous >= 2**63 - 1:
                raise ValueError("Counter exhausted")
            # Bound requests per device before signing; audit and counter commit atomically.
            recent = db.execute("SELECT count(*) FROM audit WHERE device=? AND issued>?",
                                (device_id, now - 60_000)).fetchone()[0]
            if recent >= 60:
                raise Denied("Rate limit")
            envelope = dict(zip(FIELDS, (1, device_id, uuid.uuid4().hex, grant.actor, command,
                                        key.key_epoch, previous + 1, now, now + 60_000, challenge)))
            envelope["signature"] = key.private_key.sign(canonical(envelope), ec.ECDSA(hashes.SHA256())).hex()
            db.execute("UPDATE counters SET value=? WHERE device=?", (previous + 1, device_id))
            db.execute("INSERT INTO audit VALUES (?, ?, ?, ?, ?, ?)",
                       (envelope["requestId"], device_id, grant.actor, command, previous + 1, now))
            # No envelope is returned before durable admission on the server.
        return envelope


def load_config(path: str) -> tuple[dict[str, DeviceKey], list[Grant]]:
    config_path = Path(path).resolve()
    config = json.loads(config_path.read_text())
    devices = {}
    for device, item in config["devices"].items():
        pem = (config_path.parent / item["privateKeyFile"]).read_bytes()
        private_key = serialization.load_pem_private_key(pem, password=None)
        devices[device] = DeviceKey(item["keyEpoch"], private_key, item["counterFloor"])
    grants = [Grant(g["tokenSha256"], g["expiresAtMs"], g["deviceId"], g["actor"],
                    frozenset(g["commands"])) for g in config["grants"]]
    return devices, grants
