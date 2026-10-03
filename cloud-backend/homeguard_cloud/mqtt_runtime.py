"""Long-running MQTT subscription adapter for HomeGuard cloud events.

The concrete MQTT client is injected so deployment can choose its library and
TLS credential source without coupling the domain code to a vendor package.
"""
from __future__ import annotations
from .mqtt_events import MqttEventConsumer

class MqttEventRuntime:
    def __init__(self,*,client,consumer:MqttEventConsumer):
        self.client=client
        self.consumer=consumer

    def start(self)->None:
        self.client.on_message=self._on_message
        self.client.subscribe(self.consumer.topic_filter,qos=1)
        self.client.loop_forever()

    def _on_message(self,client,userdata,message)->None:
        topic=str(message.topic)
        payload=bytes(message.payload)
        self.consumer.handle_message(topic=topic,payload=payload)
