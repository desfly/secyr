"""CI-only: run twice with one disposable Docker volume, no deployed secrets."""
import hashlib
import os
import sys
from pathlib import Path

from cryptography.hazmat.primitives.asymmetric import ec
from signer import COMMANDS, DeviceKey, Grant, Signer

assert os.getuid() == 10001, "Signer must run without root"
assert not os.access('/app', os.W_OK), "Application files must be read-only"
now = 1_800_000_000_000
key = ec.generate_private_key(ec.SECP256R1())
signer = Signer('/state/container-test.sqlite', {'HG-CI': DeviceKey(1, key, 100)},
                [Grant(hashlib.sha256(b'ci-only-token').hexdigest(), now + 120_000,
                       'HG-CI', 'ci-session', COMMANDS)], clock=lambda: now)
packet = signer.sign('ci-only-token', 'HG-CI', {'command': 'security.arm_home'})
assert packet['counter'] == int(sys.argv[1]), "Durable counter was lost across containers"
assert Path('/state/container-test.sqlite').exists()
print('Non-root signer and persistent counter PASS:', packet['counter'])
