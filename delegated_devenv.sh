#!/usr/bin/env bash
set -euo pipefail

repo_root=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)

# shellcheck source=shell/lib.sh
source "${repo_root}/shell/lib.sh"

need systemd-run
need nix

usage() {
  cat >&2 <<'EOF'
usage: delegated_devenv.sh

Runs the repo-pinned devenv inside a delegated systemd scope.

With no args, starts an interactive devenv shell.
With args, forwards them to `.forgejo/devenv_ci.sh`.

Examples:
  delegated_devenv.sh
  delegated_devenv.sh tasks run proj:devTest
EOF
  exit 2
}

if [[ ${1-} == "--help" || ${1-} == "-h" ]]; then
  usage
fi

inner_argv=( "$@" )
if [[ ${#inner_argv[@]} -eq 0 ]]; then
  inner_argv=( shell bash )
fi

if [[ ${WEBSHOT_DELEGATED_DEVENV:-} == 1 ]]; then
  cd "${repo_root}"
  exec bash ./.forgejo/devenv_ci.sh "${inner_argv[@]}"
fi

if [[ -n ${GITHUB_ACTIONS:-} ]]; then
  need sudo
  # Important: because we invoke systemd-run via sudo, "$HOME" in the systemd-run
  # process would be /root. Pass the caller's env explicitly to the delegated unit.
  exec sudo systemd-run \
    --quiet \
    --collect \
    --same-dir \
    --pipe \
    --slice=user.slice \
    --uid "$(id -u)" \
    --gid "$(id -g)" \
    --setenv=WEBSHOT_DELEGATED_DEVENV=1 \
    --setenv=PATH="${PATH}" \
    --setenv=HOME="${HOME}" \
    --setenv=TMPDIR="${TMPDIR:-/tmp}" \
    --property='Delegate=cpu memory' \
    --description="webshot delegated devenv" \
    bash -lc "cd \"\$1\"; shift; exec ./delegated_devenv.sh \"\$@\"" bash "${repo_root}" "${inner_argv[@]}"
fi

exec systemd-run \
  --user \
  --scope \
  --quiet \
  --collect \
  --same-dir \
  --setenv=WEBSHOT_DELEGATED_DEVENV=1 \
  --property='Delegate=cpu memory' \
  --description="webshot delegated devenv" \
  bash -lc "cd \"\$1\"; shift; exec ./delegated_devenv.sh \"\$@\"" bash "${repo_root}" "${inner_argv[@]}"
