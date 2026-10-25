#!/bin/sh
# Restore only into a disposable no-network container; never into the service database.
set -eu
cd "$(dirname "$0")"
dump=${1:?pass backup path relative to deploy directory}
test -s "$dump"
set -a
. ./images.env
set +a
name="traineros-backend-restore-$(date -u +%Y%m%dT%H%M%SZ)-$$"
trap 'docker rm -f "$name" >/dev/null 2>&1 || true' EXIT
docker run -d --name "$name" --network none --memory 512m --cpus 1 --pids-limit 128 \
  --tmpfs /var/lib/postgresql/data:rw,size=256m \
  -e POSTGRES_HOST_AUTH_METHOD=trust "$POSTGRES_IMAGE" >/dev/null
n=0
until docker exec "$name" pg_isready -U postgres >/dev/null 2>&1; do
  n=$((n+1)); test "$n" -lt 30; sleep 1
done
docker exec -i "$name" pg_restore -U postgres -d postgres --no-owner --no-privileges --exit-on-error < "$dump"
expected=$(docker compose exec -T db psql -U backend_owner -d traineros_backend -Atc 'SELECT version,checksum FROM trainer_backend.schema_migrations ORDER BY version' < /dev/null)
actual=$(docker exec "$name" psql -U postgres -Atc 'SELECT version,checksum FROM trainer_backend.schema_migrations ORDER BY version')
test -n "$expected"
test "$actual" = "$expected"
printf '%s\n' "$actual"
printf 'isolated_restore_ok\n'
