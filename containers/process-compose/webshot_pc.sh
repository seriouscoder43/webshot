#!/usr/bin/env bash
set -Eeuo pipefail

usage() {
  cat >&2 <<'EOF'
Usage: webshot_pc.sh <dev|prodlike> <up|down|status|logs> [process]

Required env (set by devenv):
  WEBSHOT_STATE_DIR

Required env for "up":
  WEBSHOT_BUILD_DIR
  WEBSHOT_RUNTIME_LD_LIBRARY_PATH

Optional env:
  WEBSHOT_PC_SOCKET_PATH (when set, symlinked to the webshot unix socket so plain process-compose commands work)
EOF
  exit 2
}

need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing required command: $1" >&2; exit 2; }; }
need_env() { [[ -n "${!1:-}" ]] || { echo "Missing required env var: ${1}" >&2; exit 2; }; }

mode="${1:-}"
action="${2:-}"
process_name="${3:-webshot}"

case "${mode}" in
  dev|prodlike) ;;
  *) usage ;;
esac

case "${action}" in
  up|down|status|logs) ;;
  *) usage ;;
esac

need process-compose
need timeout

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "${script_dir}/../.." && pwd)"

need_env WEBSHOT_STATE_DIR

state_dir="${WEBSHOT_STATE_DIR}/${mode}"
mkdir -p "${state_dir}"

sock="${state_dir}/process-compose.sock"
pc_server_log="${state_dir}/process-compose.server.log"

if [[ -n "${WEBSHOT_PC_SOCKET_PATH:-}" && "${WEBSHOT_PC_SOCKET_PATH}" != "${sock}" ]]; then
  mkdir -p "$(dirname -- "${WEBSHOT_PC_SOCKET_PATH}")"
  if [[ -e "${WEBSHOT_PC_SOCKET_PATH}" && ! -L "${WEBSHOT_PC_SOCKET_PATH}" ]]; then
    echo "Warning: WEBSHOT_PC_SOCKET_PATH exists and is not a symlink, refusing to overwrite: ${WEBSHOT_PC_SOCKET_PATH}" >&2
  else
    if ! ln -sf -- "${sock}" "${WEBSHOT_PC_SOCKET_PATH}"; then
      echo "Warning: failed to symlink WEBSHOT_PC_SOCKET_PATH to '${WEBSHOT_PC_SOCKET_PATH}'" >&2
    fi
  fi
fi

common_flags=(
  -U
  -u "${sock}"
  --ordered-shutdown
)

pc_is_reachable() {
  timeout 2s process-compose "${common_flags[@]}" project state >/dev/null 2>&1
}

pc_kill_server() {
  if command -v pkill >/dev/null 2>&1; then
    pkill -f "process-compose.*${sock}" >/dev/null 2>&1 || true
  fi
}

infra_down_fallback() {
  echo "Bringing down infra directly (process-compose is not running): containers/process-compose/infra_down.sh ${mode}"
  bash "containers/process-compose/infra_down.sh" "${mode}"
}

cd -- "${root}"

case "${action}" in
  up)
    need_env WEBSHOT_BUILD_DIR
    need_env WEBSHOT_RUNTIME_LD_LIBRARY_PATH
    if [[ -S "${sock}" ]]; then
      if pc_is_reachable; then
        echo "process-compose is already running: ${sock}"
        exit 0
      fi
      echo "process-compose socket exists but server is not reachable: ${sock}"
      echo "Remove the stale socket and retry:"
      echo "  rm -f '${sock}'"
      echo "See server log file (may be empty): ${pc_server_log}"
      exit 1
    fi
    echo "Starting process-compose (${mode})..."
    : >"${pc_server_log}"
    process-compose -f "containers/process-compose/${mode}.yaml" \
      "${common_flags[@]}" -L "${pc_server_log}" up -D -t=false --keep-project --logs-truncate
    for _ in {1..40}; do
      if process-compose "${common_flags[@]}" process list -o wide >/dev/null 2>&1; then
        echo "process-compose started: ${sock}"
        break
      fi
      sleep 0.1
    done
    if ! pc_is_reachable; then
      echo "process-compose server did not become available at: ${sock}"
      echo "See server log file: ${pc_server_log}"
      process-compose "${common_flags[@]}" process list -o wide || true
      exit 1
    fi

    echo "Waiting for project readiness..."
    for _ in {1..120}; do
      if [[ ! -S "${sock}" ]]; then
        echo "process-compose socket disappeared (server exited?): ${sock}"
        echo "See server log file: ${pc_server_log}"
        exit 1
      fi
      if process-compose "${common_flags[@]}" project is-ready >/dev/null 2>&1; then
        echo "Project is ready."
        exit 0
      fi
      sleep 1
    done

    echo "Project did not become ready in time."
    process-compose "${common_flags[@]}" project is-ready || true
    process-compose "${common_flags[@]}" process list -o wide || true
    echo "Infra readiness details (best-effort):"
    bash "containers/process-compose/infra_ready.sh" "${mode}" --verbose || true
    echo "See server log file: ${pc_server_log}"
    exit 1
    ;;
  down)
    if [[ ! -S "${sock}" ]]; then
      echo "process-compose is not running (no socket): ${sock}"
      infra_down_fallback
      exit $?
    fi
    if ! pc_is_reachable; then
      echo "process-compose socket exists but server is not reachable: ${sock}"
      echo "Kill the stuck server and remove the stale socket, then retry:"
      echo "  pkill -f 'process-compose -f containers/process-compose/${mode}.yaml' || true"
      echo "  rm -f '${sock}'"
      echo "See server log file (may be empty): ${pc_server_log}"
      exit 1
    fi
    echo "Stopping process-compose (${mode})..."
    pc_down_timeout_sec="${WEBSHOT_PC_DOWN_TIMEOUT_SEC:-60}"
    if ! timeout "${pc_down_timeout_sec}" process-compose "${common_flags[@]}" down; then
      echo "process-compose down timed out after ${pc_down_timeout_sec}s: ${sock}" >&2
      echo "Forcing cleanup (kill server, remove socket, bring down infra)..." >&2
      pc_kill_server
      rm -f -- "${sock}" || true
      infra_down_fallback || true
      exit 1
    fi
    if [[ -S "${sock}" && pc_is_reachable ]]; then
      echo "Warning: process-compose is still reachable after 'down': ${sock}"
      process-compose "${common_flags[@]}" project state || true
      process-compose "${common_flags[@]}" process list -o wide || true
      exit 1
    fi
    if [[ -S "${sock}" ]]; then
      echo "Warning: process-compose socket still exists after 'down': ${sock}"
      echo "If this is stale, remove it:"
      echo "  rm -f '${sock}'"
    else
      echo "process-compose stopped."
    fi
    ;;
  status)
    if [[ ! -S "${sock}" ]]; then
      echo "process-compose is not running (no socket): ${sock}"
      exit 0
    fi
    if ! pc_is_reachable; then
      echo "process-compose socket exists but server is not reachable: ${sock}"
      echo "See log file (may be empty): ${pc_server_log}"
      exit 1
    fi
    process-compose "${common_flags[@]}" process list -o wide
    ;;
  logs)
    process-compose "${common_flags[@]}" process logs --follow "${process_name}"
    ;;
esac
