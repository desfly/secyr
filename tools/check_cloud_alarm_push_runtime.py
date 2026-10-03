from pathlib import Path
import tempfile
import homeguard_cloud.alarm_push_runtime as module

class Client: pass

class FakeRuntime:
    def __init__(self,*,client,consumer):
        self.client=client
        self.consumer=consumer

def main():
    original=module.MqttEventRuntime
    module.MqttEventRuntime=FakeRuntime
    try:
        with tempfile.TemporaryDirectory() as td:
            client=Client()
            runtime=module.build_alarm_push_runtime(
                mqtt_client=client,
                project_id="homeguard-s3",
                access_token=lambda:"oauth-token",
                push_db=Path(td)/"push.db",
            )
            assert runtime.client is client
            assert runtime.consumer.topic_filter=="homeguard/v1/devices/+/events"
            service=runtime.consumer.push
            assert service.push.project_id=="homeguard-s3"
            service.tokens.set(actor="owner",device_id="HG-1",token="phone-token")
            assert service.tokens.active_tokens("HG-1")==["phone-token"]
    finally:
        module.MqttEventRuntime=original
    print("Alarm push runtime composition: OK")

if __name__=="__main__": main()
