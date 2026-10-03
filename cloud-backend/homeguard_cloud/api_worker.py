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

def main()->None:
    verifier=OidcTokenVerifier(
        issuer=_required("HOMEGUARD_OIDC_ISSUER"),
        audience=_required("HOMEGUARD_OIDC_AUDIENCE"),
        jwks_uri=_required("HOMEGUARD_OIDC_JWKS_URI"),
    )
    db=Path(os.environ.get("HOMEGUARD_CLOUD_DB","homeguard-cloud.db"))
    router=build_cloud_api(token_verifier=verifier,db_path=db)
    host=os.environ.get("HOMEGUARD_HTTP_HOST","127.0.0.1").strip() or "127.0.0.1"
    port=int(os.environ.get("HOMEGUARD_HTTP_PORT","8080"))
    if not 1<=port<=65535: raise RuntimeError("HOMEGUARD_HTTP_PORT_invalid")
    serve(router=router,host=host,port=port)

if __name__=="__main__":
    main()
