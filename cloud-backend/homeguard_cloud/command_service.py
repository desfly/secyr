"""MQTT command publication boundary.

A production adapter supplies publish(topic, payload, qos). Keeping the broker
client outside this module makes authorization/signing testable without secrets.
"""
from __future__ import annotations
import json
from typing import Protocol
from .counter_store import CounterStore
from .envelope import unsigned_envelope
from .signer import sign

class Publisher(Protocol):
    def publish(self, topic: str, payload: bytes, qos: int) -> None: ...

def command_topic(device_id: str) -> str:
    if not device_id or "/" in device_id or "+" in device_id or "#" in device_id:
        raise ValueError("invalid device_id")
    return f"homeguard/v1/devices/{device_id}/commands"

class CommandService:
    def __init__(self, counters: CounterStore, publisher: Publisher, key_epoch: int):
        if key_epoch <= 0: raise ValueError("key_epoch must be positive")
        self.counters=counters; self.publisher=publisher; self.key_epoch=key_epoch

    def send(self, *, device_id: str, actor: str, command: str, challenge: str="") -> dict:
        counter=self.counters.next(device_id)
        envelope=unsigned_envelope(device_id=device_id,actor=actor,command=command,key_epoch=self.key_epoch,counter=counter,challenge=challenge)
        signed=sign(envelope)
        payload=signed.payload()
        self.publisher.publish(command_topic(device_id),json.dumps(payload,separators=(",",":")).encode(),qos=1)
        return payload
