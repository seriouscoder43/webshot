#!/usr/bin/env bash
set -Eeuo pipefail

usage() {
  echo "Usage: infra_down.sh <dev|prodlike>" >&2
  exit 2
}

need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing required command: $1" >&2; exit 2; }; }

need podman
need podman-compose
need timeout

mode="${1:-}"
case "${mode}" in
  dev) compose_file="containers/compose/infra-dev.yaml" ;;
  prodlike) compose_file="containers/compose/infra-prodlike.yaml" ;;
  *) usage ;;
esac

timeout_sec="${WEBSHOT_INFRA_DOWN_TIMEOUT_SEC:-90}"

if timeout "${timeout_sec}" podman-compose --in-pod true -f "${compose_file}" down; then
  exit 0
fi

echo "podman-compose down timed out after ${timeout_sec}s, forcing pod removal: pod_compose" >&2
podman pod rm -f pod_compose || true
exit 1
