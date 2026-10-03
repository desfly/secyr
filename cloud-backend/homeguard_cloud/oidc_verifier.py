"""Production bearer-token verifier using OpenID Connect JWTs.

Issuer, audience and JWKS URI are deployment configuration. Verification fails
closed: signature, issuer, audience, expiry and subject are all required.
"""
from __future__ import annotations
from typing import Any

class OidcTokenVerifier:
    def __init__(self,*,issuer:str,audience:str,jwks_uri:str,jwt_module:Any=None):
        self.issuer=issuer.rstrip("/")
        self.audience=audience.strip()
        self.jwks_uri=jwks_uri.strip()
        if not self.issuer or not self.audience or not self.jwks_uri:
            raise ValueError("oidc_configuration_required")
        if jwt_module is None:
            import jwt as jwt_module
        self.jwt=jwt_module
        self.jwks=self.jwt.PyJWKClient(self.jwks_uri)

    def actor(self,bearer_token:str)->str|None:
        token=bearer_token.strip()
        if not token: return None
        try:
            key=self.jwks.get_signing_key_from_jwt(token).key
            claims=self.jwt.decode(
                token,
                key,
                algorithms=["RS256","ES256"],
                audience=self.audience,
                issuer=self.issuer,
                options={"require":["exp","iat","sub"]},
            )
        except Exception:
            return None
        subject=str(claims.get("sub","")).strip()
        return subject or None
