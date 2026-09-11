#!/usr/bin/env bash

set -euo pipefail

project_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${1:-"$project_dir/build"}
run_seconds=${2:-3}
generator="$build_dir/tools/license_generator/yoauthorize-license-generator"
service="$build_dir/src/ya_license_service/yoauthorize-service"
test_app="$build_dir/test/authorization/yoauthorize-test-app"

for executable in "$generator" "$service" "$test_app"; do
  if [[ ! -x "$executable" ]]; then
    printf 'missing executable: %s\n' "$executable" >&2
    printf 'configure with -DYOAUTHORIZE_BUILD_TOOLS=ON and build first\n' >&2
    exit 2
  fi
done

runtime=$(mktemp -d "${TMPDIR:-/tmp}/yoauthorize-demo-XXXXXX")
service_pid=
cleanup() {
  if [[ -n "$service_pid" ]] && kill -0 "$service_pid" 2>/dev/null; then
    kill -TERM "$service_pid" 2>/dev/null || true
    wait "$service_pid" 2>/dev/null || true
  fi
  rm -rf "$runtime"
}
trap cleanup EXIT INT TERM

"$generator" keygen "$runtime/issuer.key" "$runtime/issuer.pub"
"$generator" keygen "$runtime/service.key" "$runtime/service.pub"
"$generator" issue "$runtime/issuer.key" issuer-demo \
  "$runtime/license.yalc" license-demo product-demo customer-demo \
  3600 2 capture export

printf '%s\n' '0123456789abcdef0123456789abcdef' >"$runtime/machine-id"
cat >"$runtime/service.toml" <<EOF
[service]
identity_key_id = "service-demo"
identity_private_key = "$runtime/service.key"
license = "$runtime/license.yalc"
product_id = "product-demo"
socket = "$runtime/license.sock"
socket_mode = 384
allowed_uids = [$(id -u)]
heartbeat_interval_ms = 1000
heartbeat_timeout_ms = 5000

[machine_identity]
machine_id_path = "$runtime/machine-id"
product_uuid_path = "$runtime/missing-product-uuid"
require_machine_id = true
require_product_uuid = false

[[license_keys]]
id = "issuer-demo"
public_key = "$runtime/issuer.pub"
EOF

"$service" "$runtime/service.toml" >"$runtime/service.log" 2>&1 &
service_pid=$!
for _ in {1..100}; do
  if [[ -S "$runtime/license.sock" ]]; then
    break
  fi
  if ! kill -0 "$service_pid" 2>/dev/null; then
    cat "$runtime/service.log" >&2
    exit 1
  fi
  sleep 0.05
done
if [[ ! -S "$runtime/license.sock" ]]; then
  printf 'service socket was not created\n' >&2
  cat "$runtime/service.log" >&2
  exit 1
fi

"$test_app" product-demo "$runtime/license.sock" service-demo \
  "$runtime/service.pub" "$run_seconds"
