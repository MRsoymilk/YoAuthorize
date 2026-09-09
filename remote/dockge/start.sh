#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)
export DOCKGE_ROOT=$(cd "$script_dir/.." && pwd -P)
export DOCKGE_DOCKER_CONFIG="${DOCKER_CONFIG:-$HOME/.docker}"
source "$DOCKGE_ROOT/deploy/docker-network.sh"

ensure_docker_network
mkdir -p "$script_dir/.state/stacks"
if [[ ! -d "$DOCKGE_DOCKER_CONFIG" ]]; then
  DOCKGE_DOCKER_CONFIG="$script_dir/.state/docker-config"
  mkdir -p "$DOCKGE_DOCKER_CONFIG"
fi
link="$script_dir/.state/stacks/yoauthorize"
if [[ -L "$link" && "$(readlink "$link")" == "$DOCKGE_ROOT/deploy" ]]; then
  :
elif [[ -e "$link" || -L "$link" ]]; then
  printf 'Refusing to replace existing stack path: %s\n' "$link" >&2
  exit 1
else
  ln -s "$DOCKGE_ROOT/deploy" "$link"
fi

docker compose -f "$script_dir/compose.yaml" up -d --wait --wait-timeout 120
