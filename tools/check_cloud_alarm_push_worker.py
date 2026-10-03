import os, runpy, sys, types
from unittest.mock import patch

def main():
    calls={}
    class Credentials:
        valid=False; expired=True; token=None
        def refresh(self,request): self.valid=True; self.expired=False; self.token="oauth-token"
    google=types.ModuleType("google"); auth=types.ModuleType("google.auth")
    auth.default=lambda scopes:(Credentials(),"homeguard-s3")
    transport=types.ModuleType("google.auth.transport"); requests=types.ModuleType("google.auth.transport.requests")
    requests.Request=lambda: object()
    google.auth=auth
    paho=types.ModuleType("paho"); mqttpkg=types.ModuleType("paho.mqtt"); clientmod=types.ModuleType("paho.mqtt.client")
    class CallbackAPIVersion: VERSION2=2
    class Client:
        def __init__(self,*args): calls["client_args"]=args
        def username_pw_set(self,u,p): calls["auth"]=(u,p)
        def tls_set(self): calls["tls"]=True
        def connect(self,h,p,keepalive): calls["connect"]=(h,p,keepalive)
    clientmod.CallbackAPIVersion=CallbackAPIVersion; clientmod.Client=Client
    paho.mqtt=mqttpkg; mqttpkg.client=clientmod
    fake_runtime=types.ModuleType("homeguard_cloud.alarm_push_runtime")
    class Runtime:
        def start(self): calls["started"]=True
    def build_alarm_push_runtime(**kwargs):
        calls["build"]=kwargs
        assert kwargs["access_token"]()=="oauth-token"
        return Runtime()
    fake_runtime.build_alarm_push_runtime=build_alarm_push_runtime
    modules={"google":google,"google.auth":auth,"google.auth.transport":transport,"google.auth.transport.requests":requests,
             "paho":paho,"paho.mqtt":mqttpkg,"paho.mqtt.client":clientmod,
             "homeguard_cloud.alarm_push_runtime":fake_runtime}
    env={"HOMEGUARD_FIREBASE_PROJECT_ID":"homeguard-s3","HOMEGUARD_MQTT_HOST":"broker.example",
         "HOMEGUARD_MQTT_PORT":"8883","HOMEGUARD_MQTT_USERNAME":"device","HOMEGUARD_MQTT_PASSWORD":"secret",
         "HOMEGUARD_PUSH_DB":"push.db"}
    with patch.dict(sys.modules,modules), patch.dict(os.environ,env,clear=True):
        runpy.run_module("homeguard_cloud.worker",run_name="__main__")
    assert calls["auth"]==("device","secret")
    assert calls["tls"] is True
    assert calls["connect"]==("broker.example",8883,60)
    assert calls["build"]["project_id"]=="homeguard-s3"
    assert calls["started"] is True
    print("Production alarm push worker contract: OK")

if __name__=="__main__": main()
