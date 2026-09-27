#!/usr/bin/env bash
set -euo pipefail

archive=${1:?archive path required}
expected_sha256=${2:?SHA-256 required}
root=/opt/carerover/remote-hub/relay
node=/opt/node-v24.21.0-linux-x64/bin/node
service=carerover-remote-hub.service
backup_dir=/opt/carerover/backups
files=(server.mjs public/client.js public/index.html)

actual_sha256=$(sha256sum "$archive" | cut -d ' ' -f 1)
if [[ "$actual_sha256" != "$expected_sha256" ]]; then
  echo 'Audio session archive SHA-256 mismatch' >&2
  exit 1
fi
stage=$(mktemp -d /tmp/carerover-audio-session-XXXXXXXX)
tar -xf "$archive" -C "$stage"
for path in "${files[@]}"; do test -f "$stage/$path"; done
"$node" --check "$stage/server.mjs"
"$node" --check "$stage/public/client.js"

install -d -m 0750 "$backup_dir"
backup="$backup_dir/relay-before-audio-session-$(date -u +%Y%m%dT%H%M%SZ).tar"
tar -cf "$backup" -C "$root" "${files[@]}"
for path in "${files[@]}"; do
  install -o carerover -g carerover -m 0644 "$stage/$path" "$root/$path"
done
systemctl restart "$service"
systemctl is-active --quiet "$service"
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
