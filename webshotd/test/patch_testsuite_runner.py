#!/usr/bin/env python3

import argparse
from pathlib import Path

_MARKER = "# webshot testsuite port guard"


def _guard_block(service_port: int, monitor_port: int) -> str:
    return f"""
{_MARKER}
import socket
from contextlib import closing

_WEBSHOT_TESTSUITE_PORTS = ({service_port}, {monitor_port})


def _ensure_webshot_testsuite_ports_available():
    for port in _WEBSHOT_TESTSUITE_PORTS:
        with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as sock:
            sock.settimeout(0.2)
            if sock.connect_ex(("127.0.0.1", port)) == 0:
                raise SystemExit(
                    "testsuite requires exclusive localhost ports "
                    f"{{_WEBSHOT_TESTSUITE_PORTS[0]}}/{{_WEBSHOT_TESTSUITE_PORTS[1]}}; "
                    "stop any running webshot stack first"
                )


_ensure_webshot_testsuite_ports_available()
""".lstrip()


def _patch_runner(runner_path: Path, service_port: int, monitor_port: int) -> None:
    content = runner_path.read_text(encoding="utf-8")
    if _MARKER in content:
        return

    anchor = "import pytest\n"
    if anchor not in content:
        raise RuntimeError(f"failed to find patch anchor in {runner_path}")

    patched = content.replace(anchor, anchor + "\n" + _guard_block(service_port, monitor_port), 1)
    runner_path.write_text(patched, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", required=True)
    parser.add_argument("--service-port", type=int, required=True)
    parser.add_argument("--monitor-port", type=int, required=True)
    args = parser.parse_args()

    _patch_runner(
        runner_path=Path(args.runner),
        service_port=args.service_port,
        monitor_port=args.monitor_port,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
