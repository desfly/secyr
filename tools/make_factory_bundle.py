#!/usr/bin/env python3
"""Create per-device provisioning certificate, encrypted-NVS input and printable QR payload.

The output contains secrets. Generate it outside source control and keep factory-private files restricted.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import secrets
import string
import subprocess
from urllib.parse import urlencode

ALPHABET = "ABCDEFGHJKMNPQRSTUVWXYZ234567890"

def random_text(length: int) -> str:
    return ''.join(secrets.choice(ALPHABET) for _ in range(length))

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--device-id', required=True, help='HG-S3-XXXXXX')
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--setup-ip', default='192.168.4.1')
    parser.add_argument('--port', type=int, default=8443)
    args = parser.parse_args()
    if not args.device_id.startswith('HG-S3-') or len(args.device_id) != 12:
        raise SystemExit('device-id must look like HG-S3-7A31BC')
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    suffix = args.device_id.rsplit('-', 1)[-1].lower()
    operational_hostname = f'homeguard-s3-{suffix}.local'
    key = out / 'setup-key.pem'
    cert = out / 'setup-cert.pem'
    config = out / 'openssl.cnf'
    config.write_text(f'''[req]\ndistinguished_name=dn\nx509_extensions=v3\nprompt=no\n[dn]\nCN={args.device_id}\n[v3]\nsubjectAltName=IP:{args.setup_ip},DNS:{operational_hostname}\nkeyUsage=digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nbasicConstraints=CA:FALSE\n''', encoding='utf-8')
    subprocess.run(['openssl','ecparam','-name','prime256v1','-genkey','-noout','-out',str(key)], check=True)
    subprocess.run(['openssl','req','-new','-x509','-sha256','-days','3650','-key',str(key),'-out',str(cert),'-config',str(config)], check=True)
    der = subprocess.run(['openssl','x509','-in',str(cert),'-outform','DER'], check=True, capture_output=True).stdout
    fingerprint = hashlib.sha256(der).hexdigest()
    pairing_code = f'{secrets.randbelow(100_000_000):08d}'
    setup_password = random_text(16)
    setup_ssid = f'{args.device_id}-Setup'
    setup_url = f'https://{args.setup_ip}:{args.port}'
    qr = 'homeguard://provision?' + urlencode({
        'v':'1', 'id':args.device_id, 'ssid':setup_ssid, 'url':setup_url,
        'pw':setup_password, 'fp':fingerprint, 'code':pairing_code,
    })
    private = {
        'device_id': args.device_id,
        'certificate_sha256': fingerprint,
        'pairing_code': pairing_code,
        'setup_password': setup_password,
        'setup_ssid': setup_ssid,
        'setup_url': setup_url,
        'operational_hostname': operational_hostname,
        'qr_uri': qr,
    }
    private_path = out / 'factory-private.json'
    private_path.write_text(json.dumps(private, indent=2) + '\n', encoding='utf-8')
    qr_path = out / 'label-qr-uri.txt'
    qr_path.write_text(qr + '\n', encoding='utf-8')
    csv = out / 'factory-nvs.csv'
    csv.write_text(f'''key,type,encoding,value
hg-factory,namespace,,
cert_pem,file,string,{cert.name}
key_pem,file,string,{key.name}
cert_sha,data,string,{fingerprint}
pair_code,data,string,{pairing_code}
setup_pass,data,string,{setup_password}
''', encoding='utf-8')
    for path in (key, private_path, qr_path, csv):
        try: os.chmod(path, 0o600)
        except OSError: pass
    config.unlink(missing_ok=True)
    print(json.dumps({'device_id': args.device_id, 'fingerprint': fingerprint, 'output': str(out)}, indent=2))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
