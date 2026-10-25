#!/bin/sh
# Local server backup only. Off-host encrypted storage/retention remains unconfigured.
set -eu
cd "$(dirname "$0")"
umask 077
mkdir -p backups
stamp=$(date -u +%Y%m%dT%H%M%SZ)
target="backups/foundation-$stamp.dump"
test ! -e "$target"
docker compose exec -T db pg_dump -U backend_owner -d traineros_backend -Fc < /dev/null > "$target.partial"
test -s "$target.partial"
mv "$target.partial" "$target"
sha256sum "$target" > "$target.sha256"
printf '%s\n' "$target"
