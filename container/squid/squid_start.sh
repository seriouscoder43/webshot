#!/bin/sh
set -eu

# Squid TLS MITM bootstrap.

ca_crt="/usr/local/share/ca-certificates/webshot-test-target-origin-ca.crt"
if [ -r "$ca_crt" ]; then
  if command -v update-ca-certificates >/dev/null 2>&1; then
    update-ca-certificates >/dev/null
  fi
fi

find_sslcrtd() {
  for p in \
    /usr/lib/squid/security_file_certgen \
    /usr/libexec/squid/security_file_certgen \
    /usr/lib/squid/security_file_certgen64 \
    /usr/libexec/squid/security_file_certgen64
  do
    if [ -x "$p" ]; then
      echo "$p"
      return 0
    fi
  done
  return 1
}

sslcrtd="$(find_sslcrtd)" || { echo "Missing security_file_certgen" >&2; exit 2; }

if [ "$sslcrtd" != "/usr/lib/squid/security_file_certgen" ]; then
  mkdir -p /usr/lib/squid
  ln -sf "$sslcrtd" /usr/lib/squid/security_file_certgen
fi

ssl_db="/var/lib/squid/ssl_db"
ssl_db_parent="/var/lib/squid"
mkdir -p "$ssl_db_parent"

find_squid_user() {
  for u in proxy squid; do
    if id "$u" >/dev/null 2>&1; then
      echo "$u"
      return 0
    fi
  done
  return 1
}

squid_user="$(find_squid_user)" || { echo "Missing squid user (proxy/squid)" >&2; exit 2; }
chown -R "$squid_user:$squid_user" "$ssl_db_parent"

# Initialize (idempotent-ish): required for sslcrtd_program database.
rm -rf "$ssl_db" >/dev/null 2>&1 || true
sslcrtd_init_cmd="${sslcrtd} -c -s ${ssl_db} -M 16MB"
if ! su -s /bin/sh "$squid_user" -c "${sslcrtd_init_cmd}" >/dev/null; then
  echo "Failed to initialize ssl_db via: ${sslcrtd_init_cmd}" >&2
  exit 2
fi

exec squid -N -f /etc/squid/squid.conf
