from types import SimpleNamespace
from homeguard_cloud.mqtt_runtime import MqttEventRuntime

class Consumer:
    topic_filter="homeguard/v1/devices/+/events"
    def __init__(self): self.calls=[]
    def handle_message(self,**kwargs): self.calls.append(kwargs)

class Client:
    def __init__(self): self.on_message=None; self.subscriptions=[]; self.looped=False
    def subscribe(self,topic,qos): self.subscriptions.append((topic,qos))
    def loop_forever(self):
        self.looped=True
        self.on_message(self,None,SimpleNamespace(topic="homeguard/v1/devices/HG-1/events",payload=b'{"event":"alarm"}'))

def main():
    client=Client(); consumer=Consumer()
    runtime=MqttEventRuntime(client=client,consumer=consumer)
    runtime.start()
    assert client.subscriptions==[("homeguard/v1/devices/+/events",1)]
    assert client.looped
    assert consumer.calls==[{"topic":"homeguard/v1/devices/HG-1/events","payload":b'{"event":"alarm"}'}]
    print("MQTT event runtime contract: OK")

if __name__=="__main__": main()
