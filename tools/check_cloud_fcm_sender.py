import json
from homeguard_cloud.fcm_sender import FcmHttpV1Sender

class Response:
    status=200
    def __enter__(self): return self
    def __exit__(self,*args): return False

class Opener:
    def __init__(self): self.calls=[]
    def __call__(self,req,timeout):
        self.calls.append((req,timeout))
        return Response()

def main():
    opener=Opener()
    sender=FcmHttpV1Sender(project_id="homeguard-s3",access_token=lambda:"oauth-secret",opener=opener)
    sender.send(token="phone-token",data={"type":"HOMEGUARD_ALARM","event":"alarm","sequence":"7"},high_priority=True)
    assert len(opener.calls)==1
    req,timeout=opener.calls[0]
    assert req.full_url=="https://fcm.googleapis.com/v1/projects/homeguard-s3/messages:send"
    assert timeout==10
    assert req.get_method()=="POST"
    assert req.get_header("Authorization")=="Bearer oauth-secret"
    body=json.loads(req.data.decode("utf-8"))
    msg=body["message"]
    assert msg["token"]=="phone-token"
    assert msg["data"]["type"]=="HOMEGUARD_ALARM"
    assert msg["data"]["event"]=="alarm"
    assert msg["data"]["sequence"]=="7"
    assert msg["android"]["priority"]=="HIGH"
    assert "oauth-secret" not in req.data.decode("utf-8")
    print("FCM HTTP v1 sender contract: OK")

if __name__=="__main__": main()
