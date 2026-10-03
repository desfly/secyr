"""Backend-only ECDSA/SHA-256 signer for HomeGuard command envelopes."""
from __future__ import annotations
import hashlib
import os
import subprocess
import tempfile
from dataclasses import replace
from .envelope import CommandEnvelope

KEY_ENV = "HOMEGUARD_CLOUD_COMMAND_PRIVATE_KEY_PEM"

def sign(envelope: CommandEnvelope, private_key_pem: str | None = None) -> CommandEnvelope:
    key = private_key_pem if private_key_pem is not None else os.environ.get(KEY_ENV, "")
    if "PRIVATE KEY" not in key:
        raise RuntimeError(f"{KEY_ENV} is not configured")
    # OpenSSL emits ASN.1 DER ECDSA signatures. Firmware decodes hex and calls
    # mbedtls_pk_verify(..., MBEDTLS_MD_SHA256, digest, ...), so sign the same
    # SHA-256 transcript and return DER as lowercase hex.
    with tempfile.TemporaryDirectory() as d:
        key_path=os.path.join(d,"key.pem")
        data_path=os.path.join(d,"canonical.bin")
        sig_path=os.path.join(d,"signature.der")
        with open(key_path,"w",encoding="utf-8") as f: f.write(key)
        with open(data_path,"wb") as f: f.write(envelope.canonical())
        proc=subprocess.run(
            ["openssl","dgst","-sha256","-sign",key_path,"-out",sig_path,data_path],
            stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=False,
        )
        if proc.returncode != 0:
            raise RuntimeError("cloud command signing failed")
        signature=open(sig_path,"rb").read()
    if not signature:
        raise RuntimeError("empty cloud command signature")
    return replace(envelope, signature=signature.hex())
