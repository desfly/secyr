"""Production entrypoint for the authenticated HomeGuard cloud HTTP API."""
from __future__ import annotations
import os
from pathlib import Path
from .api_runtime import build_cloud_api
from .http_server import serve
from .oidc_verifier import OidcTokenVerifier

def _required(name:str)->str:
    value=os.environ.get(name,"").strip()
    if not value: raise RuntimeError(f"{name}_required")
    return value

class _MqttPublisher:
    def __init__(self,client): self.client=client
    def publish(self,topic:str,payload:bytes,qos:int)->None:
        info=self.client.publish(topic,payload,qos=qos)
        if getattr(info,"rc",0)!=0: raise RuntimeError("mqtt_publish_failed")

def main()->None:
    import paho.mqtt.client as mqtt
    verifier=OidcTokenVerifier(
        issuer=_required("HOMEGUARD_OIDC_ISSUER"),
        audience=_required("HOMEGUARD_OIDC_AUDIENCE"),
        jwks_uri=_required("HOMEGUARD_OIDC_JWKS_URI"),
    )
    db=Path(os.environ.get("HOMEGUARD_CLOUD_DB","homeguard-cloud.db"))
    mqtt_host=_required("HOMEGUARD_MQTT_HOST")
    mqtt_port=int(os.environ.get("HOMEGUARD_MQTT_PORT","8883"))
    mqtt_username=os.environ.get("HOMEGUARD_MQTT_USERNAME","").strip()
    mqtt_password=os.environ.get("HOMEGUARD_MQTT_PASSWORD","")
    client=mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    if mqtt_username: client.username_pw_set(mqtt_username,mqtt_password)
    client.tls_set()
    client.connect(mqtt_host,mqtt_port,keepalive=60)
    client.loop_start()
    router=build_cloud_api(
        token_verifier=verifier,
        db_path=db,
        command_publisher=_MqttPublisher(client),
        key_epoch=int(os.environ.get("HOMEGUARD_COMMAND_KEY_EPOCH","1")),
    )
    host=os.environ.get("HOMEGUARD_HTTP_HOST","127.0.0.1").strip() or "127.0.0.1"
    port=int(os.environ.get("HOMEGUARD_HTTP_PORT","8080"))
    if not 1<=port<=65535: raise RuntimeError("HOMEGUARD_HTTP_PORT_invalid")
    serve(router=router,host=host,port=port)

if __name__=="__main__":
    main()
