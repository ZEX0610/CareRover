#!/usr/bin/env bash
set -euo pipefail

archive=${1:?archive path required}
expected_sha256=${2:?SHA-256 required}
root=/opt/carerover/remote-hub/relay
node=/opt/node-v24.21.0-linux-x64/bin/node
service=carerover-remote-hub.service
backup_dir=/opt/carerover/backups
files=(server.mjs public/js/app.js public/js/protocol.js public/css/workspace.css)
new_files=(public/js/care-events.js public/js/care-banners.js)

actual_sha256=$(sha256sum "$archive" | cut -d ' ' -f 1)
if [[ "$actual_sha256" != "$expected_sha256" ]]; then
  echo 'Care alerts archive SHA-256 mismatch' >&2
  exit 1
fi
stage=$(mktemp -d /tmp/carerover-care-alerts-XXXXXXXX)
trap 'rm -rf -- "$stage"' EXIT
tar -xf "$archive" -C "$stage"
for path in "${files[@]}" "${new_files[@]}"; do test -f "$stage/$path"; done
for path in server.mjs public/js/app.js public/js/protocol.js public/js/care-events.js public/js/care-banners.js; do
  "$node" --check "$stage/$path"
done

install -d -m 0750 "$backup_dir"
backup="$backup_dir/relay-before-care-alerts-$(date -u +%Y%m%dT%H%M%SZ).tar"
existing_new=()
for path in "${new_files[@]}"; do
  if test -e "$root/$path"; then existing_new+=("$path"); fi
done
tar -cf "$backup" -C "$root" "${files[@]}" "${existing_new[@]}"
rollback() {
  tar -xf "$backup" -C "$root"
  chown carerover:carerover "${files[@]/#/$root/}" "${existing_new[@]/#/$root/}"
  for path in "${new_files[@]}"; do
    if [[ ! " ${existing_new[*]} " == *" $path "* ]]; then rm -f -- "$root/$path"; fi
  done
  systemctl restart "$service" || true
}
for path in "${files[@]}" "${new_files[@]}"; do
  install -o carerover -g carerover -m 0644 "$stage/$path" "$root/$path"
done
if ! systemctl restart "$service" || ! systemctl is-active --quiet "$service"; then
  rollback
  echo 'Relay failed to restart; previous version restored' >&2
  exit 1
fi
for attempt in {1..10}; do
  if curl --noproxy '*' -fsS --max-time 3 http://127.0.0.1:8088/health; then
    printf '\nbackup=%s\narchive_sha256=%s\n' "$backup" "$actual_sha256"
    exit 0
  fi
  sleep 1
done
rollback
echo "Relay unhealthy after restart; previous version restored from $backup" >&2
exit 1
