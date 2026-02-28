#!/usr/bin/env bash
set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "${script_dir}/../.." && pwd)"
# shellcheck source=shell/lib.sh
. "${root}/shell/lib.sh"

usage() {
  cat >&2 <<'EOF'
Usage: mitm_bootstrap.sh <out_dir>

Creates:
  <out_dir>/proxy_ca.crt        Public proxy CA certificate (PEM)
  <out_dir>/proxy_ca.key        Proxy CA private key (PEM, secret)
  <out_dir>/squid_ca.pem        CA key + cert (PEM) for Squid 'cert='
  <out_dir>/origin_ca.crt       Public origin CA certificate (PEM)
  <out_dir>/origin_ca.key       Origin CA private key (PEM, secret)
  <out_dir>/test_target.crt     HTTPS cert for test-target + asset.test-target (PEM, signed by origin CA)
  <out_dir>/test_target.key     HTTPS key for test-target + asset.test-target (PEM, secret)

Notes:
  - Intended for per-infra-start invocation.
  - Requires openssl on PATH.
EOF
  exit 2
}

out_dir="${1:-}"
[[ -n "${out_dir}" ]] || usage

need openssl
need mkdir
need rm

mkdir -p -- "${out_dir}"

ca_crt="${out_dir}/ca.crt"
ca_key="${out_dir}/ca.key"
squid_ca_pem="${out_dir}/squid_ca.pem"
proxy_ca_crt="${out_dir}/proxy_ca.crt"
proxy_ca_key="${out_dir}/proxy_ca.key"
origin_ca_crt="${out_dir}/origin_ca.crt"
origin_ca_key="${out_dir}/origin_ca.key"
test_target_crt="${out_dir}/test_target.crt"
test_target_key="${out_dir}/test_target.key"

if [[ -e "${ca_crt}" || -e "${ca_key}" ]]; then
  echo "Refusing to overwrite legacy MITM artifacts in ${out_dir} (ca.crt/ca.key)" >&2
  exit 2
fi

if [[ -e "${proxy_ca_crt}" || -e "${proxy_ca_key}" || -e "${squid_ca_pem}" || -e "${origin_ca_crt}" || -e "${origin_ca_key}" || -e "${test_target_crt}" || -e "${test_target_key}" ]]; then
  echo "Refusing to overwrite existing MITM artifacts in ${out_dir}" >&2
  exit 2
fi

tmp_home="$(mktemp -d)"
cleanup() {
  rm -rf -- "${tmp_home}"
}
trap cleanup EXIT

openssl req -x509 -newkey rsa:4096 -sha256 -days 7 -nodes \
  -keyout "${proxy_ca_key}" \
  -out "${proxy_ca_crt}" \
  -subj "/CN=webshot-egress-proxy-ca"

cat "${proxy_ca_key}" "${proxy_ca_crt}" >"${squid_ca_pem}"

openssl req -x509 -newkey rsa:4096 -sha256 -days 7 -nodes \
  -keyout "${origin_ca_key}" \
  -out "${origin_ca_crt}" \
  -subj "/CN=webshot-test-target-origin-ca"

# HTTPS test targets: generate an origin cert signed by a dedicated origin CA.
#
# Browser should not trust the origin CA; Squid should. Browser trusts only the
# proxy CA and relies on ssl-bump for HTTPS.
test_target_csr="${tmp_home}/test_target.csr"
test_target_ext="${tmp_home}/test_target.ext"
cat >"${test_target_ext}" <<'EOF'
[v3_req]
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = test-target
DNS.2 = asset.test-target
EOF

openssl req -new -newkey rsa:2048 -sha256 -nodes \
  -keyout "${test_target_key}" \
  -out "${test_target_csr}" \
  -subj "/CN=test-target"

openssl x509 -req -sha256 -days 7 \
  -in "${test_target_csr}" \
  -CA "${origin_ca_crt}" \
  -CAkey "${origin_ca_key}" \
  -CAserial "${tmp_home}/origin_ca.srl" \
  -CAcreateserial \
  -out "${test_target_crt}" \
  -extfile "${test_target_ext}" \
  -extensions v3_req
