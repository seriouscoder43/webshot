#!/usr/bin/env bash
set -euo pipefail

containers_dest="${XDG_CONFIG_HOME:-"${HOME}/.config"}/containers/systemd"
user_units_dest="${XDG_CONFIG_HOME:-"${HOME}/.config"}/systemd/user"
runtime_dest=""
if [[ -n "${XDG_RUNTIME_DIR:-}" ]]; then
  runtime_dest="${XDG_RUNTIME_DIR}/containers/systemd"
fi

mkdir -p "${containers_dest}" "${user_units_dest}"
if [[ -n "${runtime_dest}" ]]; then
  mkdir -p "${runtime_dest}"
fi

root="${PWD}"
echo "Installing Webshot Quadlet units (symlinks) into: ${containers_dest}"

echo "Setting user systemd environment: WEBSHOT_ROOT=${root}"
systemctl --user set-environment "WEBSHOT_ROOT=${root}"

for unit in containers/quadlet/webshot-crawler.network \
            containers/quadlet/webshot-postgres.container \
            containers/quadlet/webshot-seaweed.container \
            containers/quadlet/webshot-scalar.container \
            containers/quadlet/webshot-test-target.container \
            containers/quadlet/webshot-reverse-proxy.container; do
  if [[ ! -f "${unit}" ]]; then
    echo "Skipping missing unit template: ${unit}" >&2
    continue
  fi
  name="$(basename "${unit}")"
  # Symlink from Quadlet search paths back into the repo.
  target="${containers_dest}/${name}"
  ln -sf "${root}/${unit}" "${target}"
  echo "Linked ${unit} -> ${target}"

  if [[ -n "${runtime_dest}" ]]; then
    rt_target="${runtime_dest}/${name}"
    ln -sf "${root}/${unit}" "${rt_target}"
    echo "Linked ${unit} -> ${rt_target}"
  fi
done

stack_unit_src="containers/quadlet/webshot-stack.target"
if [[ -f "${stack_unit_src}" ]]; then
  stack_target="${user_units_dest}/webshot-stack.target"
  ln -sf "${root}/${stack_unit_src}" "${stack_target}"
  echo "Linked ${stack_unit_src} -> ${stack_target}"
fi

echo "Reloading user systemd units"
systemctl --user daemon-reload
