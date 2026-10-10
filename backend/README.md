# HomeGuard command signer

This server returns signed firmware v1 envelopes from
`POST /v1/devices/<deviceId>/signed-command` with a bearer account token.
The JSON request contains `command` and optionally `challenge`; actor, device
binding, epoch, counter and timestamps come from server configuration/storage.
The response is the envelope accepted by Android `SignedCommandEnvelope.parse`.

It does not publish to MQTT, deploy itself, enroll trust on ESP, create firmware
actor sessions, or configure Android. These integration steps remain pending.
Firmware verifies every signature and maintains its own durable replay barrier.

## Run

Use Python 3.12 and install `backend/requirements.txt`, then run:

```sh
python backend/server.py --config /secure/signer.json --database /persistent/signer.sqlite
```

The default listener is `127.0.0.1:9080`. Place it behind an authenticated TLS
reverse proxy for remote access. Keep PEM keys/configuration private and preserve
the SQLite database and its audit records across restarts and deployments.

Configuration structure (placeholders, not working enrollment credentials):

```json
{
  "devices": {
    "HG-DEVICE": {
      "keyEpoch": 1,
      "privateKeyFile": "command-signing-key.pem",
      "counterFloor": 123
    }
  },
  "grants": [{
    "tokenSha256": "<64 lowercase hex characters: SHA-256 of account bearer token>",
    "expiresAtMs": 1800000000000,
    "deviceId": "HG-DEVICE",
    "actor": "<existing controller-authorized actor/session>",
    "commands": ["security.arm_home", "security.arm_away", "security.disarm", "output.lock"]
  }]
}
```

Use an EC private key matching the public key already enrolled in the controller's
Cloud Command Trust, and the same active key epoch. No real keys or tokens are
included. `counterFloor` must be at least the controller's persisted command
counter; obtain it explicitly during enrollment/recovery. The database retains
the larger existing counter, including across key rotation. Losing both its
counter and the controller counter information blocks safe recovery; never guess
or reset the controller replay state.

Grant tokens must be unique, random account credentials, delivered through an
account authentication flow; broker passwords and device PINs are not these
tokens. Revocation currently requires removing the grant and restarting the
service. Grant expiry is enforced. The actor must have an active controller
session; this server does not create or refresh it.

Each device is limited to 60 signed commands per minute. Counter allocation and
successful signing audit commit before returning a packet. Requests for one device
must be delivered/executed in counter order by the eventual command coordinator;
concurrent clients can otherwise publish a newer counter before an older packet,
which the firmware correctly rejects. On MQTT timeout the result is unknown: do
not automatically issue another lock/disarm command.

For disarm, request and execute `security.disarm_challenge` first, then submit the
controller's returned challenge in the signed `security.disarm` request. Signing
an arbitrary challenge does not make it valid; the firmware checks its one-time
challenge. The response transport/coordinator is still to be integrated.

## Verification

```sh
PYTHONPATH=backend python -m unittest discover -s backend/tests -v
```

Tests cover the exact firmware signature transcript, tamper rejection,
account/device/permission isolation, durable counters, parallel allocation,
rate limiting, disarm challenge shape and a real local HTTP request. They use
new temporary keys/databases and do not contact an ESP or broker.
