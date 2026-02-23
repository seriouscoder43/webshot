#!/usr/bin/env bash

die() {
  echo "$*" >&2
  exit 2
}

need() {
  command -v "$1" >/dev/null 2>&1 || die "Missing required command: $1"
}

need_env() {
  [[ -n "${!1:-}" ]] || die "Missing required env var: ${1}"
}

