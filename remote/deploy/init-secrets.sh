#!/usr/bin/env bash

set -euo pipefail

deploy_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
secret_dir="$deploy_dir/secrets"
if [[ -e "$secret_dir" ]]; then
  printf 'Refusing to overwrite existing secrets directory: %s\n' "$secret_dir" >&2
  exit 1
fi
mkdir -m 700 "$secret_dir"

database_password=$(openssl rand -hex 24)
openssl rand -hex 32 >"$secret_dir/activation-pepper"
openssl rand -hex 32 >"$secret_dir/signer-shared-secret"
openssl rand 32 >"$secret_dir/license-signing-key"
openssl rand 24 | openssl base64 -A >"$secret_dir/bootstrap-admin-password"
printf '%s' "$database_password" >"$secret_dir/postgres-password"
printf 'postgres://yoauthorize:%s@postgres:5432/yoauthorize' \
  "$database_password" >"$secret_dir/database-url"
chmod 644 "$secret_dir/activation-pepper" "$secret_dir/signer-shared-secret" \
  "$secret_dir/postgres-password" "$secret_dir/database-url"
chmod 600 "$secret_dir/license-signing-key" "$secret_dir/bootstrap-admin-password"
if [[ ! -e "$deploy_dir/.env" ]]; then
  printf 'LOCAL_UID=%s\nLOCAL_GID=%s\n' "$(id -u)" "$(id -g)" >"$deploy_dir/.env"
fi
printf 'Development secrets created under %s\n' "$secret_dir"
