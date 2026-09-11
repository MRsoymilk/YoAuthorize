#!/usr/bin/env bash

set -euo pipefail

DOCKER_NETWORK="${DOCKER_NETWORK:-dev-net}"
DOCKER_BRIDGE="${DOCKER_BRIDGE:-br-docker}"

ensure_docker_network() {
  if docker network inspect "$DOCKER_NETWORK" >/dev/null 2>&1; then
    local driver
    driver=$(docker network inspect -f '{{.Driver}}' "$DOCKER_NETWORK")
    if [[ "$driver" != "bridge" ]]; then
      printf "ERROR: Docker network '%s' uses driver '%s'; expected 'bridge'.\n" \
        "$DOCKER_NETWORK" "$driver" >&2
      return 1
    fi
    printf '[Docker] Using network: %s\n' "$DOCKER_NETWORK"
    return
  fi

  printf '[Docker] Creating network: %s\n' "$DOCKER_NETWORK"
  docker network create \
    --driver bridge \
    --opt "com.docker.network.bridge.name=$DOCKER_BRIDGE" \
    "$DOCKER_NETWORK"
}
