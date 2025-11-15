#!/bin/sh
set -eu

echo "[netpol] Installing iptables..."
# Alpine 3.20: ip6tables provided by iptables package
apk add --no-cache iptables >/dev/null 2>&1 || true

if ! command -v iptables >/dev/null 2>&1; then
  echo "[netpol] iptables unavailable; sleeping for inspection" >&2
  exec sleep infinity
fi

echo "[netpol] Applying egress rules (idempotent)"
# Block DNS/DoT
iptables -C OUTPUT -p udp --dport 53 -j REJECT 2>/dev/null || iptables -I OUTPUT -p udp --dport 53 -j REJECT || true
iptables -C OUTPUT -p tcp --dport 53 -j REJECT 2>/dev/null || iptables -I OUTPUT -p tcp --dport 53 -j REJECT || true
iptables -C OUTPUT -p tcp --dport 853 -j REJECT 2>/dev/null || iptables -I OUTPUT -p tcp --dport 853 -j REJECT || true

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
  iptables -C OUTPUT -d "$c" -j REJECT 2>/dev/null || iptables -I OUTPUT -d "$c" -j REJECT || true
done

# IPv6 policy (if IPv6 is enabled in the netns)
if command -v ip6tables >/dev/null 2>&1; then
  for c in \
    ::/128 \
    ::1/128 \
    fc00::/7 \
    fe80::/10 \
    ::ffff:0:0/96 \
    ff00::/8; do
    ip6tables -C OUTPUT -d "$c" -j REJECT 2>/dev/null || ip6tables -I OUTPUT -d "$c" -j REJECT || true
  done
fi

echo "[netpol] Rules applied; keeping container alive"
exec sleep infinity

