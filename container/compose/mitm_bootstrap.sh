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
  <out_dir>/ca.crt              Public proxy CA certificate (PEM)
  <out_dir>/ca.key              Proxy CA private key (PEM, secret)
  <out_dir>/squid_ca.pem        CA key + cert (PEM) for Squid 'cert='
  <out_dir>/nssdb.tar.gz        Tarball containing .pki/nssdb (NSS SQL DB) that trusts ca.crt

Notes:
  - Intended for per-infra-start invocation.
  - Requires openssl + certutil (from nssTools) on PATH.
EOF
  exit 2
}

out_dir="${1:-}"
[[ -n "${out_dir}" ]] || usage

need openssl
need certutil
need tar
need mkdir
need rm

mkdir -p -- "${out_dir}"

ca_crt="${out_dir}/ca.crt"
ca_key="${out_dir}/ca.key"
squid_ca_pem="${out_dir}/squid_ca.pem"
nssdb_tar="${out_dir}/nssdb.tar.gz"

if [[ -e "${ca_crt}" || -e "${ca_key}" || -e "${squid_ca_pem}" || -e "${nssdb_tar}" ]]; then
  echo "Refusing to overwrite existing MITM artifacts in ${out_dir}" >&2
  exit 2
fi

tmp_home="$(mktemp -d)"
cleanup() {
  rm -rf -- "${tmp_home}"
}
trap cleanup EXIT

openssl req -x509 -newkey rsa:4096 -sha256 -days 7 -nodes \
  -keyout "${ca_key}" \
  -out "${ca_crt}" \
  -subj "/CN=webshot-egress-proxy-ca"

cat "${ca_key}" "${ca_crt}" >"${squid_ca_pem}"

# Create an NSS SQL DB under ${tmp_home}/.pki/nssdb and trust the new CA.
db_dir="${tmp_home}/.pki/nssdb"
mkdir -p -- "${db_dir}"

certutil -N -d "sql:${db_dir}" --empty-password
certutil -A -d "sql:${db_dir}" -n "webshot-egress-proxy-ca" -t "C,," -i "${ca_crt}"

tar -C "${tmp_home}" -czf "${nssdb_tar}" .pki
