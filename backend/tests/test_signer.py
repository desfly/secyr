import concurrent.futures
import hashlib
import http.client
import json
import sqlite3
import tempfile
import threading
import unittest
from unittest.mock import patch
from pathlib import Path
from http.server import ThreadingHTTPServer

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec

from signer import COMMANDS, Denied, DeviceKey, Grant, Signer, canonical
from server import handler_for


class SignerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.database = str(Path(self.temp.name) / 'state.sqlite')
        self.key = ec.generate_private_key(ec.SECP256R1())
        self.now = 1_800_000_000_000
        self.devices = {'HG-TEST': DeviceKey(3, self.key, 100)}
        self.grants = [Grant(hashlib.sha256(b'test-token').hexdigest(), self.now + 120_000,
                             'HG-TEST', 'controller-session', COMMANDS)]
        self.signer = Signer(self.database, self.devices, self.grants, clock=lambda: self.now)

    def sign(self, request=None, token='test-token', device='HG-TEST'):
        return self.signer.sign(token, device, request or {'command': 'security.arm_away'})

    def test_signature_matches_firmware_transcript_and_detects_tampering(self):
        envelope = self.sign()
        expected = ('version=1\ndeviceId=HG-TEST\nrequestId=' + envelope['requestId'] +
                    '\nactor=controller-session\ncommand=security.arm_away\nkeyEpoch=3\ncounter=101' +
                    f'\nissuedAtMs={self.now}\nexpiresAtMs={self.now + 60_000}\nchallenge=')
        self.assertEqual(expected.encode(), canonical(envelope))
        signature = bytes.fromhex(envelope['signature'])
        self.key.public_key().verify(signature, canonical(envelope), ec.ECDSA(hashes.SHA256()))
        envelope['command'] = 'security.disarm'
        with self.assertRaises(InvalidSignature):
            self.key.public_key().verify(signature, canonical(envelope), ec.ECDSA(hashes.SHA256()))

    def test_counter_survives_restart_and_never_rolls_back_with_lower_floor(self):
        self.assertEqual(101, self.sign()['counter'])
        self.devices['HG-TEST'] = DeviceKey(4, self.key, 0)
        self.signer = Signer(self.database, self.devices, self.grants, clock=lambda: self.now)
        self.assertEqual(102, self.sign()['counter'])

    def test_parallel_requests_get_unique_durable_counters(self):
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            packets = list(pool.map(lambda _: self.sign(), range(20)))
        self.assertEqual(list(range(101, 121)), sorted(p['counter'] for p in packets))
        self.assertEqual(20, len({p['requestId'] for p in packets}))
        with sqlite3.connect(self.database) as db:
            self.assertEqual(20, db.execute('SELECT count(*) FROM audit').fetchone()[0])

    def test_unknown_expired_and_cross_device_tokens_are_rejected(self):
        for token, device in [('bad', 'HG-TEST'), ('test-token', 'HG-OTHER')]:
            with self.assertRaises(Denied):
                self.sign(token=token, device=device)
        self.now += 120_000
        with self.assertRaises(Denied):
            self.sign()

    def test_actor_pin_counter_and_command_permission_cannot_be_overridden(self):
        for name in ['actor', 'credential', 'counter', 'keyEpoch']:
            with self.assertRaises(ValueError):
                self.sign({'command': 'security.arm_away', name: 'injected'})
        restricted = [Grant(self.grants[0].token_sha256, self.now + 10_000,
                            'HG-TEST', 'controller-session', frozenset({'security.arm_home'}))]
        self.signer = Signer(self.database, self.devices, restricted, clock=lambda: self.now)
        with self.assertRaises(Denied):
            self.sign()

    def test_disarm_requires_signed_challenge(self):
        for challenge in ['', None, 'bad']:
            with self.assertRaises(ValueError):
                self.sign({'command': 'security.disarm', 'challenge': challenge})
        value = '0123456789abcdef0123456789abcdef'
        self.assertEqual(value, self.sign({'command': 'security.disarm', 'challenge': value})['challenge'])

    def test_failed_signing_does_not_commit_counter_or_audit(self):
        with patch('signer.canonical', side_effect=RuntimeError('signing unavailable')):
            with self.assertRaises(RuntimeError):
                self.sign()
        with sqlite3.connect(self.database) as db:
            self.assertEqual(100, db.execute('SELECT value FROM counters').fetchone()[0])
            self.assertEqual(0, db.execute('SELECT count(*) FROM audit').fetchone()[0])
        self.assertEqual(101, self.sign()['counter'])

    def test_rate_limit_does_not_allocate_another_counter(self):
        for _ in range(60):
            self.sign()
        with self.assertRaises(Denied):
            self.sign()
        with sqlite3.connect(self.database) as db:
            self.assertEqual(160, db.execute('SELECT value FROM counters').fetchone()[0])

    def test_http_endpoint_requires_account_binding_and_returns_signed_packet(self):
        server = ThreadingHTTPServer(('127.0.0.1', 0), handler_for(self.signer))
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            for token, status in [('bad', 403), ('test-token', 200)]:
                connection = http.client.HTTPConnection('127.0.0.1', server.server_port, timeout=3)
                connection.request('POST', '/v1/devices/HG-TEST/signed-command',
                                   json.dumps({'command': 'security.arm_away'}),
                                   {'Content-Type': 'application/json', 'Authorization': 'Bearer ' + token})
                response = connection.getresponse()
                self.assertEqual(status, response.status)
                body = json.loads(response.read())
                if status == 200:
                    self.key.public_key().verify(bytes.fromhex(body['signature']), canonical(body),
                                                 ec.ECDSA(hashes.SHA256()))
                connection.close()
        finally:
            server.shutdown()
            server.server_close()
            thread.join()


if __name__ == '__main__':
    unittest.main()
