#!/usr/bin/env bash

infra_mitm_bootstrap_if_needed() {
  local script_dir="$1"
  local mode="$2"
  local mitm_link="/tmp/webshot_mitm"

  if bash "${script_dir}/infra_ready.sh" "${mode}" >/dev/null 2>&1; then
    return 0
  fi

  local old_target
  local new_target
  old_target="$(readlink "${mitm_link}" 2>/dev/null || true)"
  new_target="$(mktemp -d /tmp/webshot_mitm.XXXXXX)"
  bash "${script_dir}/mitm_bootstrap.sh" "${new_target}"

  ln -sfn "${new_target}" "${mitm_link}.new"
  mv -Tf "${mitm_link}.new" "${mitm_link}"

  if [[ -n "${old_target}" && "${old_target}" != "${new_target}" && "${old_target}" == /tmp/webshot_mitm.* ]]; then
    rm -rf -- "${old_target}" || true
  fi
}

infra_down_compose_and_cleanup_mitm() {
  local script_dir="$1"
  local compose_file="$2"

  local mitm_link="/tmp/webshot_mitm"
  local mitm_target
  mitm_target="$(readlink "${mitm_link}" 2>/dev/null || true)"

  cd -- "${script_dir}" || return 1

  local timeout_sec="${WEBSHOT_INFRA_DOWN_TIMEOUT_SEC:-90}"
  if timeout "${timeout_sec}" compose --in-pod true -f "${compose_file}" down; then
    rm -f -- "${mitm_link}" || true
    if [[ -n "${mitm_target}" && "${mitm_target}" == /tmp/webshot_mitm.* ]]; then
      rm -rf -- "${mitm_target}" || true
    fi
    return 0
  fi

  if ! podman pod inspect pod_compose >/dev/null 2>&1; then
    echo "Infra is already down (pod 'pod_compose' not found)." >&2
    rm -f -- "${mitm_link}" || true
    if [[ -n "${mitm_target}" && "${mitm_target}" == /tmp/webshot_mitm.* ]]; then
      rm -rf -- "${mitm_target}" || true
    fi
    return 0
  fi

  echo "Warning: podman-compose down failed/timed out after ${timeout_sec}s; forcing pod removal: pod_compose" >&2
  podman pod rm -f pod_compose || true
  if ! podman pod inspect pod_compose >/dev/null 2>&1; then
    rm -f -- "${mitm_link}" || true
    if [[ -n "${mitm_target}" && "${mitm_target}" == /tmp/webshot_mitm.* ]]; then
      rm -rf -- "${mitm_target}" || true
    fi
  fi

  return 0
}

