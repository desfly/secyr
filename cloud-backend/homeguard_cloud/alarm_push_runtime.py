"""Compose the HomeGuard alarm-push cloud process from environment/config.

Network libraries and credential providers stay injectable so this module is
testable without contacting MQTT or Google.
"""
from __future__ import annotations
from pathlib import Path
from .event_push import EventPushService
from .fcm_sender import FcmHttpV1Sender
from .mqtt_events import MqttEventConsumer
from .mqtt_runtime import MqttEventRuntime
from .push_store import PushTokenStore

def build_alarm_push_runtime(*,mqtt_client,project_id:str,access_token,push_db:str|Path)->MqttEventRuntime:
    tokens=PushTokenStore(push_db)
    sender=FcmHttpV1Sender(project_id=project_id,access_token=access_token)
    service=EventPushService(tokens,sender)
    consumer=MqttEventConsumer(service)
    return MqttEventRuntime(client=mqtt_client,consumer=consumer)
