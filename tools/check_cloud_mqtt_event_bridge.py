from homeguard_cloud.mqtt_events import MqttEventConsumer

class Push:
    def __init__(self): self.calls=[]
    def handle(self,**kwargs):
        self.calls.append(kwargs)
        return 2

def main():
    push=Push(); consumer=MqttEventConsumer(push)
    payload=b'{"event":"alarm","seq":9}'
    assert consumer.topic_filter=="homeguard/v1/devices/+/events"
    assert consumer.handle_message(topic="homeguard/v1/devices/HG-ACA7041DA710/events",payload=payload)==2
    assert push.calls==[{"device_id":"HG-ACA7041DA710","payload":payload}]
    assert consumer.handle_message(topic="homeguard/v1/devices/HG-ACA7041DA710/status",payload=payload)==0
    assert consumer.handle_message(topic="homeguard/v1/devices/HG-1/events/extra",payload=payload)==0
    assert len(push.calls)==1
    print("MQTT event to push bridge contract: OK")

if __name__=="__main__": main()
