#!/usr/bin/env bash
set -euo pipefail

cmd="${1:-}"
net_name="${2:-}"
if [[ -z "${cmd}" ]]; then
  echo "Usage: $0 {apply|clear} <docker-network-name> | {apply-cidr|clear-cidr} <v4cidr> [v6cidr]" >&2
  exit 2
fi

# Resolve IPv4/IPv6 CIDRs for given Docker network name
resolve_cidrs() {
  local name="$1"
  local inspect
  inspect=$(docker network inspect "$name" --format '{{json .IPAM.Config}}')
  if [[ -z "$inspect" || "$inspect" == "null" ]]; then
    echo ""; echo ""; return 0
  fi
  local v4="" v6=""
  # shellcheck disable=SC2207
  local subnets=($(echo "$inspect" | jq -r '.[].Subnet // empty'))
  for s in "${subnets[@]:-}"; do
    case "$s" in
      *:*) v6="$s" ;;
      *)   v4="$s" ;;
    esac
  done
  echo "$v4"; echo "$v6"
}

apply_rules() {
  local cidr_v4="$1"; local cidr_v6="$2"
  # Block DNS/DoT
  if [[ -n "$cidr_v4" ]]; then
    iptables -C DOCKER-USER -s "$cidr_v4" -p udp --dport 53 -j REJECT 2>/dev/null || iptables -I DOCKER-USER -s "$cidr_v4" -p udp --dport 53 -j REJECT
    iptables -C DOCKER-USER -s "$cidr_v4" -p tcp --dport 53 -j REJECT 2>/dev/null || iptables -I DOCKER-USER -s "$cidr_v4" -p tcp --dport 53 -j REJECT
    iptables -C DOCKER-USER -s "$cidr_v4" -p tcp --dport 853 -j REJECT 2>/dev/null || iptables -I DOCKER-USER -s "$cidr_v4" -p tcp --dport 853 -j REJECT
  fi
  # Block IPv4 private/bogon ranges
  for c in \
    0.0.0.0/8 \
    10.0.0.0/8 \
    100.64.0.0/10 \
    127.0.0.0/8 \
    169.254.0.0/16 \
    172.16.0.0/12 \
    192.168.0.0/16 \
    224.0.0.0/4 \
    240.0.0.0/4; do
    if [[ -n "$cidr_v4" ]]; then
      iptables -C DOCKER-USER -s "$cidr_v4" -d "$c" -j REJECT 2>/dev/null || iptables -I DOCKER-USER -s "$cidr_v4" -d "$c" -j REJECT
    fi
  done
  # IPv6 optional (only if ip6tables present)
  if command -v ip6tables >/dev/null 2>&1; then
    for c in \
      ::/128 \
      ::1/128 \
      fc00::/7 \
      fe80::/10 \
      ::ffff:0:0/96 \
      ff00::/8; do
      if [[ -n "$cidr_v6" ]]; then
        ip6tables -C DOCKER-USER -s "$cidr_v6" -d "$c" -j REJECT 2>/dev/null || ip6tables -I DOCKER-USER -s "$cidr_v6" -d "$c" -j REJECT
      fi
    done
  fi
}

clear_rules() {
  local cidr_v4="$1"; local cidr_v6="$2"
  # Best-effort delete; ignore errors
  if [[ -n "$cidr_v4" ]]; then
    iptables -D DOCKER-USER -s "$cidr_v4" -p udp --dport 53 -j REJECT 2>/dev/null || true
    iptables -D DOCKER-USER -s "$cidr_v4" -p tcp --dport 53 -j REJECT 2>/dev/null || true
    iptables -D DOCKER-USER -s "$cidr_v4" -p tcp --dport 853 -j REJECT 2>/dev/null || true
  fi
  for c in 0.0.0.0/8 10.0.0.0/8 100.64.0.0/10 127.0.0.0/8 169.254.0.0/16 172.16.0.0/12 192.168.0.0/16 224.0.0.0/4 240.0.0.0/4; do
    if [[ -n "$cidr_v4" ]]; then
      iptables -D DOCKER-USER -s "$cidr_v4" -d "$c" -j REJECT 2>/dev/null || true
    fi
  done
  if command -v ip6tables >/dev/null 2>&1; then
    for c in ::/128 ::1/128 fc00::/7 fe80::/10 ::ffff:0:0/96 ff00::/8; do
      if [[ -n "$cidr_v6" ]]; then
        ip6tables -D DOCKER-USER -s "$cidr_v6" -d "$c" -j REJECT 2>/dev/null || true
      fi
    done
  fi
}

case "${cmd}" in
  apply)
    if [[ -z "${net_name}" ]]; then
      echo "Network name is required" >&2
      exit 2
    fi
    mapfile -t cidrs < <(resolve_cidrs "$net_name")
    cidr_v4="${cidrs[0]}"; cidr_v6="${cidrs[1]}"
    apply_rules "$cidr_v4" "$cidr_v6"
    echo "Applied DOCKER-USER egress policy scoped to $net_name ($cidr_v4 ${cidr_v6:-})"
    ;;
  clear)
    if [[ -z "${net_name}" ]]; then
      echo "Network name is required" >&2
      exit 2
    fi
    mapfile -t cidrs < <(resolve_cidrs "$net_name")
    cidr_v4="${cidrs[0]}"; cidr_v6="${cidrs[1]}"
    clear_rules "$cidr_v4" "$cidr_v6"
    echo "Cleared DOCKER-USER rules for $net_name ($cidr_v4 ${cidr_v6:-})"
    ;;
  apply-cidr)
    # Usage: apply-cidr <v4cidr> [v6cidr]
    apply_rules "${net_name}" "${3:-}"
    echo "Applied DOCKER-USER policy for CIDR(s) ${net_name} ${3:-}"
    ;;
  clear-cidr)
    # Usage: clear-cidr <v4cidr> [v6cidr]
    clear_rules "${net_name}" "${3:-}"
    echo "Cleared DOCKER-USER policy for CIDR(s) ${net_name} ${3:-}"
    ;;
  *)
    echo "Unknown command: ${cmd}" >&2
    exit 2
    ;;
esac
