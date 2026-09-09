#!/usr/bin/env bash

set -euo pipefail

deploy_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "$deploy_dir/docker-network.sh"

ensure_docker_network
docker compose --project-directory "$deploy_dir" -f "$deploy_dir/compose.yaml" up "$@"
