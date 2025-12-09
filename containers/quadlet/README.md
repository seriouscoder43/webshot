This directory contains example Podman Quadlet units for the Webshot
development stack. The units mirror `containers/testing-compose.yaml`
using systemd + Quadlet instead of Docker Compose.

Usage (rootless, per-user):

- Ensure `podman` and `systemd --user` are available on the host.
- From the Webshot repo root, run the Devenv task that calls
  `containers/quadlet/install_quadlet.sh` (see `devenv.nix`) to install
  or update the units under `~/.config/containers/systemd/`.
- Start the full stack via the Devenv task that runs
  `systemctl --user start webshot-stack.target`.
- Stop the stack via the Devenv task that runs
  `systemctl --user stop webshot-stack.target`.

Notes:

- The `webshot-crawler.network` unit enables `NetworkDeleteOnStop=true`
  so `systemctl --user stop webshot-crawler-network.service` removes
  the Podman network, similar to `docker compose down`.
- The `crawler-netkeeper` container from `testing-compose.yaml` is not
  modelled here; the dedicated network is managed via the `.network`
  unit instead.

