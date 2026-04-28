#!/usr/bin/env python3

import argparse
import ast
import pprint
from pathlib import Path

_MARKER = "# webshot testsuite port guard"
_PYTEST_ARGS_NAME = "TESTSUITE_PYTEST_ARGS"


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
                    "testsuite refuses to run while localhost ports "
                    f"{{_WEBSHOT_TESTSUITE_PORTS[0]}}/{{_WEBSHOT_TESTSUITE_PORTS[1]}}; "
                    "stop any running webshot dev stack first"
                )


_ensure_webshot_testsuite_ports_available()
""".lstrip()


def _without_fixed_service_log(content: str) -> str:
    tree = ast.parse(content)
    for node in tree.body:
        if not isinstance(node, ast.Assign):
            continue
        if not any(
            isinstance(target, ast.Name) and target.id == _PYTEST_ARGS_NAME
            for target in node.targets
        ):
            continue
        pytest_args = ast.literal_eval(node.value)
        filtered_args = [
            arg
            for arg in pytest_args
            if not (isinstance(arg, str) and arg.startswith("--service-logs-file="))
        ]
        if filtered_args == pytest_args:
            return content

        lines = content.splitlines(keepends=True)
        replacement = f"{_PYTEST_ARGS_NAME} = {pprint.pformat(filtered_args)}\n"
        lines[node.lineno - 1 : node.end_lineno] = [replacement]
        return "".join(lines)

    raise RuntimeError(f"failed to find {_PYTEST_ARGS_NAME} in generated testsuite runner")


def _patch_runner(runner_path: Path, service_port: int, monitor_port: int) -> None:
    content = runner_path.read_text(encoding="utf-8")
    content = _without_fixed_service_log(content)
    if _MARKER in content:
        runner_path.write_text(content, encoding="utf-8")
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
