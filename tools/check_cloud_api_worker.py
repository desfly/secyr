from __future__ import annotations
import os,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"cloud-backend"))

import homeguard_cloud.api_worker as worker

old=dict(os.environ)
try:
    for key in ("HOMEGUARD_OIDC_ISSUER","HOMEGUARD_OIDC_AUDIENCE","HOMEGUARD_OIDC_JWKS_URI"):
        os.environ.pop(key,None)
    try:
        worker.main()
        raise AssertionError("worker started without OIDC configuration")
    except RuntimeError as exc:
        assert str(exc)=="HOMEGUARD_OIDC_ISSUER_required"

    os.environ["HOMEGUARD_OIDC_ISSUER"]="https://id.example"
    os.environ["HOMEGUARD_OIDC_AUDIENCE"]="homeguard-api"
    os.environ["HOMEGUARD_OIDC_JWKS_URI"]="https://id.example/jwks"
    os.environ["HOMEGUARD_HTTP_HOST"]="127.0.0.1"
    os.environ["HOMEGUARD_HTTP_PORT"]="18080"

    seen={}
    class Verifier:
        def __init__(self,**kwargs): seen["oidc"]=kwargs
    worker.OidcTokenVerifier=Verifier
    worker.build_cloud_api=lambda **kwargs: seen.setdefault("api",kwargs) or object()
    def fake_serve(**kwargs):
        seen["serve"]=kwargs
    worker.serve=fake_serve
    worker.main()
    assert seen["oidc"]["audience"]=="homeguard-api"
    assert seen["serve"]["host"]=="127.0.0.1"
    assert seen["serve"]["port"]==18080
finally:
    os.environ.clear(); os.environ.update(old)

print("cloud api worker: ok")
