#!/usr/bin/env sh
set -eu

: "${HOMEGUARD_CLOUD_DOMAIN:?Set HOMEGUARD_CLOUD_DOMAIN}"
: "${HOMEGUARD_MQTT_USER:?Set HOMEGUARD_MQTT_USER}"
: "${HOMEGUARD_MQTT_PASSWORD:?Set HOMEGUARD_MQTT_PASSWORD}"

rm -f passwords
docker run --rm \
  -v "$(pwd):/work" \
  eclipse-mosquitto:2 \
  mosquitto_passwd -b -c /work/passwords "$HOMEGUARD_MQTT_USER" "$HOMEGUARD_MQTT_PASSWORD"
chmod 600 passwords

docker compose up -d
printf 'HomeGuard Cloud starting at wss://%s/mqtt\n' "$HOMEGUARD_CLOUD_DOMAIN"
