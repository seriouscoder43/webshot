#!/usr/bin/env bash
set -Eeuo pipefail

usage() {
  echo "Usage: infra_daemon.sh <dev|prodlike>" >&2
  exit 2
}

need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing required command: $1" >&2; exit 2; }; }

need podman-compose
need timeout

mode="${1:-}"
case "${mode}" in
  dev) compose_file="containers/compose/infra-dev.yaml" ;;
  prodlike) compose_file="containers/compose/infra-prodlike.yaml" ;;
  *) usage ;;
esac

bash containers/compose/ensure_networks.sh

up_ok="false"
for attempt in 1 2 3; do
  if podman-compose --in-pod true -f "${compose_file}" up -d; then
    up_ok="true"
    break
  fi
  echo "podman-compose up failed (attempt ${attempt}/3)" >&2
  sleep 2
done

if [[ "${up_ok}" != "true" ]]; then
  echo "podman-compose up failed after 3 attempts, exiting so process-compose can restart infra." >&2
  exit 1
fi

trap 'exit 0' INT TERM
while true; do
  sleep 3600
done
