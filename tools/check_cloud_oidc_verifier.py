from __future__ import annotations
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"cloud-backend"))
from homeguard_cloud.oidc_verifier import FirebaseTokenVerifier

class Auth:
    @staticmethod
    def verify_id_token(token,app=None,check_revoked=False):
        assert check_revoked is False
        if token=="bad": raise ValueError("bad token")
        if token=="wrong-aud": return {"sub":"account-7","aud":"other","iss":"https://securetoken.google.com/homeguard-s3"}
        if token=="wrong-iss": return {"sub":"account-7","aud":"homeguard-s3","iss":"https://evil.example"}
        if token=="no-sub": return {"sub":"","aud":"homeguard-s3","iss":"https://securetoken.google.com/homeguard-s3"}
        return {"sub":"account-7","aud":"homeguard-s3","iss":"https://securetoken.google.com/homeguard-s3"}

try:
    FirebaseTokenVerifier(project_id="",auth_module=Auth)
    raise AssertionError("empty project id accepted")
except ValueError:
    pass

verifier=FirebaseTokenVerifier(project_id="homeguard-s3",auth_module=Auth)
assert verifier.actor("") is None
assert verifier.actor("bad") is None
assert verifier.actor("wrong-aud") is None
assert verifier.actor("wrong-iss") is None
assert verifier.actor("no-sub") is None
assert verifier.actor("valid")=="account-7"
print("cloud Firebase verifier: ok")
