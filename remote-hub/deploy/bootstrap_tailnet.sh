#!/usr/bin/env bash
# One-time private demo configuration. Run as root on the relay host.
# Never prints generated credentials or replaces an existing configuration.
set -euo pipefail

origin="${1:?Pass the exact https://<relay>.<tailnet>.ts.net origin}"
if [[ "$origin" != https://*.ts.net || "$origin" == */ ]]; then
  echo 'Expected an exact HTTPS ts.net origin without a trailing slash' >&2
  exit 2
fi
if [[ "$(id -u)" != 0 ]]; then
  echo 'Run as root' >&2
  exit 2
fi
if [[ -e /etc/carerover/remote-hub.env ]]; then
  echo 'Existing /etc/carerover/remote-hub.env preserved; no changes made' >&2
  exit 3
fi
getent group carerover >/dev/null
install -d -o root -g carerover -m 0750 /etc/carerover
umask 077
tmp="$(mktemp /etc/carerover/remote-hub.env.XXXXXXXX)"
trap 'rm -f -- "$tmp"' EXIT
device_token="$(openssl rand -hex 32)"
parent_token="$(openssl rand -hex 32)"
{
  printf 'AUDIO_DEVICE_TOKEN=%s\n' "$device_token"
  printf 'AUDIO_PARENT_TOKEN=%s\n' "$parent_token"
  printf 'AUDIO_PROXY_PUBLIC_ORIGIN=%s\n' "$origin"
  printf 'PORT=8088\n'
} > "$tmp"
chown root:carerover "$tmp"
chmod 0640 "$tmp"
mv -- "$tmp" /etc/carerover/remote-hub.env
trap - EXIT
echo 'Private relay configuration created; token values were not printed.'
