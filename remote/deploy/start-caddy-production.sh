#!/bin/sh
set -eu

# PUBLIC_URL is an external HTTPS DNS origin, not the container listener.
PUBLIC_URL=${PUBLIC_URL%/}
case "$PUBLIC_URL" in
  https://*) authority=${PUBLIC_URL#https://} ;;
  *) printf '%s\n' 'Production PUBLIC_URL must be an HTTPS DNS origin.' >&2; exit 1 ;;
esac
host=${authority%%:*}
port=443
case "$host" in
  ''|*[!a-zA-Z0-9.-]*|-*|.*|*.) printf '%s\n' 'Invalid production PUBLIC_URL hostname.' >&2; exit 1 ;;
esac
if [ "$authority" != "$host" ]; then
  port=${authority#*:}
  case "$port" in
    ''|*[!0-9]*|??????*) printf '%s\n' 'Invalid production PUBLIC_URL port.' >&2; exit 1 ;;
  esac
  if [ "$port" -lt 1 ] || [ "$port" -gt 65535 ]; then
    printf '%s\n' 'Invalid production PUBLIC_URL port.' >&2; exit 1
  fi
fi
export PUBLIC_URL
export CADDY_PUBLIC_HTTPS_PORT="$port"
export CADDY_SITE_ADDRESS="https://$host:443"
exec caddy run --config /etc/caddy/Caddyfile --adapter caddyfile
