"""MQTT topic adapter for HomeGuard device events.

The broker client owns networking/reconnects. This adapter validates the
canonical topic and forwards the payload to EventPushService.
"""
from __future__ import annotations
import re
from .event_push import EventPushService

_EVENT_TOPIC=re.compile(r"^homeguard/v1/devices/([^/]+)/events$")

class MqttEventConsumer:
    topic_filter="homeguard/v1/devices/+/events"

    def __init__(self,push:EventPushService):
        self.push=push

    def handle_message(self,*,topic:str,payload:bytes)->int:
        match=_EVENT_TOPIC.fullmatch(topic)
        if not match:
            return 0
        device_id=match.group(1).strip()
        if not device_id:
            return 0
        return self.push.handle(device_id=device_id,payload=payload)
