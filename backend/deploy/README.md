# HTTPS deployment

This package is ready for a Linux host with Docker Engine and Docker Compose.
It has not been deployed to a real host. Before starting, obtain a public DNS
name pointing to that host, with TCP ports 80/443 reachable, and the configuration
described in `../README.md`. Android uses `https://<your-domain>/` as the service URL.

The signer listens only inside a private container network. Caddy exposes HTTPS
and obtains/renews certificates. Bearer account authorization stays in the signer;
the proxy does not replace it. Access logs are not enabled. The application runs
as UID/GID 10001 with a read-only filesystem and a writable persistent state volume.

## Prepare and start

Place `signer.json` and its referenced PEM files in a directory outside this Git
checkout, for example `/srv/homeguard-signer/keys`. Paths in `privateKeyFile` are
relative to that directory. The existing enrolled public key, current key epoch,
actual replay counter floor and controller-authorized actor session are required.
Do not invent them or erase the controller to obtain a clean counter.

On Linux, let container GID 10001 read that directory/files, for example directory
mode 0750 and file mode 0640 with group 10001. Do not make secrets world-readable.
No private key, account token or live configuration is supplied by this package.

Run from `backend/deploy` after substituting your domain and directory:

```sh
export SIGNER_DOMAIN=your-real-domain.example
export SIGNER_SECRETS_DIR=/srv/homeguard-signer/keys
docker volume create homeguard_signer_state
docker volume create homeguard_signer_caddy_data
docker compose config --quiet
docker compose up -d --build
docker compose ps
```

The two explicitly named external volumes survive container replacement and
`docker compose down`. Keep their names stable. The signer has no published port;
only the HTTPS service binds host ports. The build context includes application
code and dependency declarations only, excluding configuration and keys.

The sample domain is a placeholder and cannot receive a working public certificate.
Use a real DNS name before `up`. Image tags track Python 3.12 and Caddy 2; production
operators can pin verified image digests under their update policy.

## Update, revoke and recover

After a reviewed code update, `docker compose up -d --build` replaces containers
while preserving counters/audit data. To revoke account access, remove its grant
from `signer.json` and run `docker compose restart signer`. This does not revoke a
packet already issued before restart; its lifetime is at most 60 seconds.

For a consistent state backup, stop the signer and copy the entire
`homeguard_signer_state` volume using your host's volume backup procedure, then
start it again. Keep keys/configuration and certificate data in separate protected
backups. Never copy an active SQLite database file without SQLite's backup API.
Restoring an older database requires reconciling `counterFloor` with the current
controller replay state before issuing another command; never reset that state.

Multiple phones still require coordinated delivery in counter order. A successful
HTTPS response means a signed packet was issued, not that a controller executed it.
Android reports the MQTT response; an unknown result must not trigger an automatic
repeat of lock or disarm. This package does not provision/refresh controller actor
sessions or Cloud Command Trust.

## Validation

CI checks Compose expansion, Caddy configuration and the real signer container.
It runs as UID 10001 with a read-only root and signs once in each of two replacement
containers sharing one disposable volume. The counters must be 101 then 102.
Only temporary CI keys/tokens are used; no deployed host, broker or ESP is contacted.

References: [Caddy HTTPS](https://caddyserver.com/docs/quick-starts/https),
[Compose services](https://docs.docker.com/reference/compose-file/services/),
[Docker volumes](https://docs.docker.com/engine/storage/volumes/).
