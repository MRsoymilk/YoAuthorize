#!/usr/bin/env bash

set -euo pipefail

project_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${1:-"$project_dir/build"}
mode=${2:-all}
generator="$build_dir/tools/license_generator/yoauthorize-license-generator"
qt_app="$build_dir/samples/TestYoQt/TestYoQt"
fake_server="$project_dir/test/activation/fake_activation_server.py"

if [[ ! -x "$generator" || ! -x "$qt_app" ]]; then
  printf 'Build tools and samples before running this demo.\n' >&2
  exit 2
fi
if [[ "$mode" != "local" && "$mode" != "local-denied" &&
      "$mode" != "remote" && "$mode" != "remote-denied" &&
      "$mode" != "all" ]]; then
  printf 'mode must be local, local-denied, remote, remote-denied, or all\n' >&2
  exit 2
fi

runtime=$(mktemp -d "${TMPDIR:-/tmp}/yoauthorize-qt-demo-XXXXXX")
server_pid=
cleanup() {
  if [[ -n "$server_pid" ]] && kill -0 "$server_pid" 2>/dev/null; then
    kill -TERM "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
  fi
  rm -rf "$runtime"
}
trap cleanup EXIT INT TERM

"$generator" keygen "$runtime/issuer.key" "$runtime/issuer.pub"
"$generator" issue "$runtime/issuer.key" issuer-qt \
  "$runtime/license.yalc" license-qt product-qt customer-qt \
  3600 1 capture export
printf '%s\n' '0123456789abcdef0123456789abcdef' >"$runtime/machine-id"

common_args=(
  --product product-qt
  --issuer-key-id issuer-qt
  --issuer-public-key "$runtime/issuer.pub"
  --machine-id-file "$runtime/machine-id"
  --auto-exit
)

if [[ "$mode" == "local" || "$mode" == "all" ]]; then
  QT_QPA_PLATFORM=offscreen "$qt_app" "${common_args[@]}" \
    --license "$runtime/license.yalc"
fi

if [[ "$mode" == "local-denied" ]]; then
  common_args[1]=wrong-product
  QT_QPA_PLATFORM=offscreen "$qt_app" "${common_args[@]}" \
    --license "$runtime/license.yalc"
fi

if [[ "$mode" == "remote" || "$mode" == "remote-denied" ||
      "$mode" == "all" ]]; then
  command -v python3 >/dev/null
  command -v openssl >/dev/null
  openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
    -subj "/CN=localhost" \
    -addext "subjectAltName=DNS:localhost" \
    -addext "basicConstraints=critical,CA:TRUE" \
    -keyout "$runtime/https.key" -out "$runtime/https.crt" \
    >"$runtime/openssl.log" 2>&1
  python3 "$fake_server" \
    --certificate "$runtime/https.crt" \
    --private-key "$runtime/https.key" \
    --license "$runtime/license.yalc" \
    --port-file "$runtime/https.port" \
    --product-id product-qt \
    --activation-code activate-qt \
    >"$runtime/server.log" 2>&1 &
  server_pid=$!
  for _ in {1..100}; do
    if [[ -s "$runtime/https.port" ]]; then
      break
    fi
    if ! kill -0 "$server_pid" 2>/dev/null; then
      printf 'Fake activation server failed to start.\n' >&2
      exit 1
    fi
    sleep 0.05
  done
  if [[ ! -s "$runtime/https.port" ]]; then
    printf 'Fake activation server did not publish a port.\n' >&2
    exit 1
  fi
  port=$(<"$runtime/https.port")
  activation_code=activate-qt
  if [[ "$mode" == "remote-denied" ]]; then
    activation_code=wrong-code
  fi
  QT_QPA_PLATFORM=offscreen "$qt_app" "${common_args[@]}" \
    --activation-url "https://localhost:$port/v1/activate" \
    --activation-code "$activation_code" \
    --ca-certificate "$runtime/https.crt"
fi
