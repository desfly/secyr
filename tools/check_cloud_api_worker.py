from __future__ import annotations
import os,sys,types
from unittest.mock import patch
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"cloud-backend"))

import homeguard_cloud.api_worker as worker

old=dict(os.environ)
try:
    os.environ.pop("HOMEGUARD_FIREBASE_PROJECT_ID",None)
    try:
        worker.main()
        raise AssertionError("worker started without OIDC configuration")
    except RuntimeError as exc:
        assert str(exc)=="HOMEGUARD_FIREBASE_PROJECT_ID_required"

    os.environ["HOMEGUARD_FIREBASE_PROJECT_ID"]="homeguard-s3"
    os.environ["HOMEGUARD_HTTP_HOST"]="127.0.0.1"
    os.environ["HOMEGUARD_HTTP_PORT"]="18080"
    os.environ["HOMEGUARD_MQTT_HOST"]="broker.example"

    seen={}
    class CallbackAPIVersion: VERSION2=2
    class Client:
        def __init__(self,*args): seen["mqtt_client"]=self
        def username_pw_set(self,u,p): seen["mqtt_auth"]=(u,p)
        def tls_set(self): seen["mqtt_tls"]=True
        def connect(self,h,p,keepalive): seen["mqtt_connect"]=(h,p,keepalive)
        def loop_start(self): seen["mqtt_loop"]=True
        def publish(self,*args,**kwargs): return types.SimpleNamespace(rc=0)
    mqtt=types.ModuleType("paho.mqtt.client"); mqtt.CallbackAPIVersion=CallbackAPIVersion; mqtt.Client=Client
    paho=types.ModuleType("paho"); mqttpkg=types.ModuleType("paho.mqtt"); paho.mqtt=mqttpkg; mqttpkg.client=mqtt
    sys.modules["paho"]=paho; sys.modules["paho.mqtt"]=mqttpkg; sys.modules["paho.mqtt.client"]=mqtt
    class Verifier:
        def __init__(self,**kwargs): seen["firebase"]=kwargs
    worker.FirebaseTokenVerifier=Verifier
    worker.build_cloud_api=lambda **kwargs: seen.setdefault("api",kwargs) or object()
    def fake_serve(**kwargs):
        seen["serve"]=kwargs
    worker.serve=fake_serve
    worker.main()
    assert seen["firebase"]["project_id"]=="homeguard-s3"
    assert seen["serve"]["host"]=="127.0.0.1"
    assert seen["serve"]["port"]==18080
    assert seen["mqtt_connect"]==("broker.example",8883,60)
    assert seen["mqtt_loop"] is True
    assert "command_publisher" in seen["api"]
finally:
    os.environ.clear(); os.environ.update(old)

print("cloud api worker: ok")
