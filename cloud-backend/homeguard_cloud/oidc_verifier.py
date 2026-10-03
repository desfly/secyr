"""Firebase ID-token verifier for HomeGuard cloud API.

Production uses the Firebase Admin SDK instead of treating Firebase's X.509
certificate endpoint as a generic JWKS document.
"""
from __future__ import annotations
from typing import Any

class FirebaseTokenVerifier:
    def __init__(self,*,project_id:str,auth_module:Any=None,app:Any=None):
        self.project_id=project_id.strip()
        if not self.project_id:
            raise ValueError("firebase_project_id_required")
        if auth_module is None:
            from firebase_admin import auth as auth_module
        self.auth=auth_module
        self.app=app

    def actor(self,bearer_token:str)->str|None:
        token=bearer_token.strip()
        if not token:
            return None
        try:
            claims=self.auth.verify_id_token(
                token,
                app=self.app,
                check_revoked=False,
            )
        except Exception:
            return None
        audience=str(claims.get("aud","")).strip()
        issuer=str(claims.get("iss","")).rstrip("/")
        subject=str(claims.get("sub",claims.get("uid",""))).strip()
        if audience!=self.project_id:
            return None
        if issuer!=f"https://securetoken.google.com/{self.project_id}":
            return None
        return subject or None

# Kept as an import-compatible alias while deployment/tests migrate names.
OidcTokenVerifier=FirebaseTokenVerifier
