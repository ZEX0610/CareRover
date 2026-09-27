#!/usr/bin/env bash
set -euo pipefail

# Apply only the reviewed R5 relay UI files. Keep a restorable server-side tarball.
archive=${1:?archive path required}
expected_sha256=${2:?SHA-256 required}
root=/opt/carerover/remote-hub/relay
node=/opt/node-v24.21.0-linux-x64/bin/node
service=carerover-remote-hub.service
backup_dir=/opt/carerover/backups
files=(
  server.mjs
  public/console.html
  public/css/remote.css
  public/css/workspace.css
  public/js/app.js
  public/js/front-panel.js
  public/js/i18n.js
  public/js/telemetry.js
  public/js/video.js
  public/js/workspace.js
)

actual_sha256=$(sha256sum "$archive" | cut -d ' ' -f 1)
if [[ "$actual_sha256" != "$expected_sha256" ]]; then
  echo 'R5 archive SHA-256 mismatch' >&2
  exit 1
fi

stage=$(mktemp -d /tmp/carerover-r5-XXXXXXXX)
tar -xf "$archive" -C "$stage"
for path in "${files[@]}"; do
  test -f "$stage/$path"
done
"$node" --check "$stage/server.mjs"
"$node" --check "$stage/public/js/app.js"

install -d -m 0750 "$backup_dir"
backup="$backup_dir/relay-ui-before-r5-$(date -u +%Y%m%dT%H%M%SZ).tar"
tar -cf "$backup" -C "$root" \
  server.mjs public/console.html public/css/remote.css \
  public/js/app.js public/js/front-panel.js public/js/i18n.js \
  public/js/telemetry.js public/js/video.js

for path in "${files[@]}"; do
  install -o carerover -g carerover -m 0644 "$stage/$path" "$root/$path"
done
systemctl restart "$service"
systemctl is-active --quiet "$service"
"$node" --check "$root/server.mjs"
printf 'backup=%s\narchive_sha256=%s\n' "$backup" "$actual_sha256"
for attempt in {1..10}; do
  if curl --noproxy '*' -fsS --max-time 3 http://127.0.0.1:8088/health; then
    printf '\n'
    exit 0
  fi
  sleep 1
done
echo 'Relay did not become healthy after restart' >&2
exit 1
