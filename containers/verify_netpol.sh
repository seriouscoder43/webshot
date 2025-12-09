#!/usr/bin/env bash
set -Eeuo pipefail

NETWORK_NAME="${NETWORK_NAME:-crawler_net}"

while getopts ":n:h" opt; do
  case ${opt} in
    n) NETWORK_NAME="$OPTARG" ;;
    h)
      echo "Usage: $0 [-n <network-name>]" ; exit 0 ;;
    :) echo "Option -$OPTARG requires an argument" >&2 ; exit 2 ;;
    \?) echo "Unknown option: -$OPTARG" >&2 ; exit 2 ;;
  esac
done

bold() { printf "\033[1m%s\033[0m\n" "$*"; }
green() { printf "\033[32m%s\033[0m\n" "$*"; }
red() { printf "\033[31m%s\033[0m\n" "$*"; }
yellow() { printf "\033[33m%s\033[0m\n" "$*"; }

need() { command -v "$1" >/dev/null 2>&1 || { red "Missing required command: $1"; exit 2; }; }

need podman

bold "Verifying crawler egress policy (connectivity only)"
echo "- Network:       ${NETWORK_NAME}"
echo "- Runtime:       podman (user=$(id -un))"

# Launch tester container on crawler network
TESTER="netpol-tester-$(date +%s)"
cleanup() {
  podman rm -f "$TESTER" >/dev/null 2>&1 || true
}
trap cleanup EXIT

podman run \
  --hooks-dir ./containers \
  --annotation webshot.crawler.netpol=true \
  -d --rm --name "$TESTER" --network "$NETWORK_NAME" \
  alpine:3.20 sh -lc 'sleep infinity' >/dev/null

pass_count=0
fail_count=0

expect_fail() {
  local label="$1"; shift
  if podman exec -i "$TESTER" sh -lc "$*" >/dev/null 2>&1; then
    red "FAIL (should block): $label"
    ((fail_count++))
  else
    green "OK   (blocked):    $label"
    ((pass_count++))
  fi
}

expect_ok() {
  local label="$1"; shift
  if podman exec -i "$TESTER" sh -lc "$*" >/dev/null 2>&1; then
    green "OK   (allowed):    $label"
    ((pass_count++))
  else
    red "FAIL (should allow): $label"
    ((fail_count++))
  fi
}

bold "Running live egress checks from ${TESTER} on ${NETWORK_NAME}…"

# Blocked: DNS over UDP via public resolver
expect_fail "UDP/53 to 1.1.1.1" "nc -uz -w3 1.1.1.1 53"

# Blocked: TCP/53 and TCP/853
expect_fail "TCP/53 to 1.1.1.1" "nc -z -w3 1.1.1.1 53"
expect_fail "TCP/853 (DoT) to 1.1.1.1" "nc -z -w3 1.1.1.1 853"

# Blocked: DNS resolution using explicit server (should timeout)
expect_fail "DNS lookup via 1.1.1.1" "nslookup -timeout=2 example.com 1.1.1.1"

# Allowed: Direct HTTPS by IP (no DNS), using busybox wget
expect_ok "HTTPS to 1.1.1.1 (no DNS)" "wget -q -T5 -O- https://1.1.1.1/cdn-cgi/trace | head -n1"

# Blocked: Private/bogon ranges (sample address on a private CIDR)
expect_fail "HTTP to private address (10.0.0.1:80)" "wget -q -T3 -O- http://10.0.0.1/" || true

bold "Summary: $pass_count passed, $fail_count failed"
if [[ $fail_count -gt 0 ]]; then
  exit 1
fi
exit 0
