"""Production entrypoint for the HomeGuard alarm-push worker.

Secrets are read from the environment. Google credentials use Application
Default Credentials; no private key belongs in the repository.
"""
from __future__ import annotations
import os
from pathlib import Path

def _required(name:str)->str:
    value=os.environ.get(name,"").strip()
    if not value: raise RuntimeError(f"{name}_required")
    return value

def main()->None:
    import google.auth
    from google.auth.transport.requests import Request
    import paho.mqtt.client as mqtt
    from .alarm_push_runtime import build_alarm_push_runtime

    project_id=_required("HOMEGUARD_FIREBASE_PROJECT_ID")
    host=_required("HOMEGUARD_MQTT_HOST")
    port=int(os.environ.get("HOMEGUARD_MQTT_PORT","8883"))
    username=os.environ.get("HOMEGUARD_MQTT_USERNAME","").strip()
    password=os.environ.get("HOMEGUARD_MQTT_PASSWORD","")
    push_db=Path(os.environ.get("HOMEGUARD_PUSH_DB","homeguard-push.db"))

    credentials,adc_project=google.auth.default(scopes=["https://www.googleapis.com/auth/firebase.messaging"])
    if not project_id and adc_project:
        project_id=adc_project

    def access_token()->str:
        if not credentials.valid or credentials.expired:
            credentials.refresh(Request())
        return str(credentials.token or "")

    client=mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    if username:
        client.username_pw_set(username,password)
    client.tls_set()
    client.connect(host,port,keepalive=60)

    runtime=build_alarm_push_runtime(
        mqtt_client=client,
        project_id=project_id,
        access_token=access_token,
        push_db=push_db,
    )
    runtime.start()

if __name__=="__main__":
    main()
