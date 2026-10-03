from __future__ import annotations
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"cloud-backend"))
from homeguard_cloud.oidc_verifier import OidcTokenVerifier

class Key:
    key="public-key"
class Jwks:
    def __init__(self,uri): self.uri=uri
    def get_signing_key_from_jwt(self,token):
        if token=="bad-signature": raise ValueError("bad")
        return Key()
class Jwt:
    PyJWKClient=Jwks
    @staticmethod
    def decode(token,key,**kwargs):
        assert key=="public-key"
        assert kwargs["algorithms"]==["RS256","ES256"]
        assert kwargs["audience"]=="homeguard-api"
        assert kwargs["issuer"]=="https://id.example"
        assert set(kwargs["options"]["require"])=={"exp","iat","sub"}
        if token=="expired": raise ValueError("expired")
        if token=="no-sub": return {"sub":""}
        return {"sub":"account-7","exp":9999999999,"iat":1}

try:
    OidcTokenVerifier(issuer="",audience="x",jwks_uri="y",jwt_module=Jwt)
    raise AssertionError("empty issuer accepted")
except ValueError:
    pass

verifier=OidcTokenVerifier(issuer="https://id.example/",audience="homeguard-api",jwks_uri="https://id.example/jwks",jwt_module=Jwt)
assert verifier.actor("") is None
assert verifier.actor("bad-signature") is None
assert verifier.actor("expired") is None
assert verifier.actor("no-sub") is None
assert verifier.actor("valid")=="account-7"
print("cloud OIDC verifier: ok")
