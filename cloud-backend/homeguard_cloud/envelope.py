"""Canonical HomeGuard cloud-command envelope builder.

The signing key stays on the backend. Android sends an authenticated command
request; the backend authorizes the account/device binding, allocates the
monotonic counter, signs this envelope, then publishes it to the device MQTT
commands topic.
"""
from __future__ import annotations
from dataclasses import dataclass, asdict
from time import time
from uuid import uuid4

MAX_TTL_MS = 120_000

@dataclass(frozen=True)
class CommandEnvelope:
    version: int
    deviceId: str
    requestId: str
    actor: str
    command: str
    keyEpoch: int
    counter: int
    issuedAtMs: int
    expiresAtMs: int
    challenge: str
    signature: str = ""

    def canonical(self) -> bytes:
        fields = (
            ("version", self.version), ("deviceId", self.deviceId),
            ("requestId", self.requestId), ("actor", self.actor),
            ("command", self.command), ("keyEpoch", self.keyEpoch),
            ("counter", self.counter), ("issuedAtMs", self.issuedAtMs),
            ("expiresAtMs", self.expiresAtMs), ("challenge", self.challenge),
        )
        return "\n".join(f"{k}={v}" for k, v in fields).encode("utf-8")

    def payload(self) -> dict:
        return asdict(self)

def unsigned_envelope(*, device_id: str, actor: str, command: str,
                      key_epoch: int, counter: int, challenge: str = "",
                      ttl_ms: int = MAX_TTL_MS, now_ms: int | None = None) -> CommandEnvelope:
    if not device_id or not actor or not command:
        raise ValueError("device_id, actor and command are required")
    if key_epoch <= 0 or counter <= 0:
        raise ValueError("key_epoch and counter must be positive")
    if ttl_ms <= 0 or ttl_ms > MAX_TTL_MS:
        raise ValueError("invalid ttl")
    issued = int(time() * 1000) if now_ms is None else now_ms
    return CommandEnvelope(1, device_id, uuid4().hex, actor, command,
                           key_epoch, counter, issued, issued + ttl_ms,
                           challenge, "")
