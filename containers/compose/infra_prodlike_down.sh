#!/usr/bin/env bash
set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
. "${script_dir}/podman_compose_helpers.sh"

need podman
need podman-compose

cd -- "${script_dir}"
compose_file="infra-prodlike.yaml"

podman-compose --in-pod true -f "${compose_file}" down
