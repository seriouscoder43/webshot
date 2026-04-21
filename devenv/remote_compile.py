#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import shlex
import subprocess
from dataclasses import dataclass
from pathlib import Path

_DEFAULT_CONFIG_PATH = Path(".devenv/remote_compile.json")
_RESULT_MARKER = "__REMOTE_BUILD_RESULT__"


@dataclass(frozen=True)
class RemoteCompileConfig:
    host: str
    remote_root: str
    ssh_args: tuple[str, ...]
    rsync_args: tuple[str, ...]

    def fingerprint_payload(self) -> dict[str, object]:
        return {
            "host": self.host,
            "remote_root": self.remote_root,
            "ssh_args": list(self.ssh_args),
            "rsync_args": list(self.rsync_args),
        }


@dataclass(frozen=True)
class RemoteBuildResult:
    configure_exit_code: int
    build_exit_code: int
    configure_time_ms: int
    build_time_ms: int


def load_config(repo_root: Path, *, path: str | None = None) -> RemoteCompileConfig | None:
    config_path = repo_root / (path or _DEFAULT_CONFIG_PATH.as_posix())
    if not config_path.is_file():
        return None

    raw = json.loads(config_path.read_text(encoding="utf-8"))
    if not isinstance(raw, dict):
        raise RuntimeError(f"remote compile config must be an object: {config_path}")

    host = raw.get("host")
    if not isinstance(host, str) or not host:
        raise RuntimeError("remote compile config must define string `host`")

    remote_root = raw.get("remote_root")
    if not isinstance(remote_root, str) or not remote_root:
        raise RuntimeError("remote compile config must define string `remote_root`")
    if not remote_root.startswith("/") or remote_root == "/":
        raise RuntimeError("remote compile config `remote_root` must be an absolute non-root path")

    return RemoteCompileConfig(
        host=host,
        remote_root=remote_root.rstrip("/"),
        ssh_args=_read_string_tuple(raw.get("ssh_args"), field_name="ssh_args"),
        rsync_args=_read_string_tuple(raw.get("rsync_args"), field_name="rsync_args"),
    )


def compute_configure_fingerprint(
    configure_spec_fingerprint: str,
    config: RemoteCompileConfig | None,
) -> str:
    if config is None:
        return configure_spec_fingerprint

    payload = json.dumps(
        {
            "configure_spec_fingerprint": configure_spec_fingerprint,
            "remote_compile": config.fingerprint_payload(),
        },
        separators=(",", ":"),
        sort_keys=True,
    )
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def sync_source(config: RemoteCompileConfig, repo_root: Path) -> None:
    _ensure_remote_dir(config, config.remote_root)
    cmd = [
        "rsync",
        "-a",
        "--delete",
        "--filter=:- .gitignore",
        "--exclude=/.git/",
        *config.rsync_args,
        _with_trailing_slash(repo_root.as_posix()),
        f"{config.host}:{_with_trailing_slash(config.remote_root)}",
    ]
    _ensure_ok(_run(cmd), action="remote source sync")


def sync_build_dir(config: RemoteCompileConfig, *, repo_root: Path, build_dir: Path) -> None:
    remote_build_dir = remote_path_for(
        build_dir,
        repo_root=repo_root,
        remote_root=config.remote_root,
    )
    build_dir.mkdir(parents=True, exist_ok=True)
    cmd = [
        "rsync",
        "-a",
        "--delete",
        *config.rsync_args,
        f"{config.host}:{_with_trailing_slash(remote_build_dir)}",
        _with_trailing_slash(build_dir.as_posix()),
    ]
    _ensure_ok(_run(cmd), action="remote build sync")


def run_remote_build(
    config: RemoteCompileConfig,
    *,
    configure_cmd: list[str],
    build_cmd: list[str],
) -> RemoteBuildResult:
    remote_script = " ".join(
        [
            "set -euo pipefail;",
            f"cd -- {shlex.quote(config.remote_root)};",
            "exec ./delegated_devenv.sh",
        ]
    )
    inner_script = _remote_build_script(configure_cmd=configure_cmd, build_cmd=build_cmd)
    completed = subprocess.Popen(
        ["ssh", *config.ssh_args, config.host, "bash", "-lc", remote_script],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    assert completed.stdin is not None
    assert completed.stdout is not None
    completed.stdin.write(inner_script)
    completed.stdin.close()

    result: RemoteBuildResult | None = None
    for line in completed.stdout:
        if line.startswith(_RESULT_MARKER):
            result = _parse_result_line(line)
            continue
        print(line, end="", flush=True)

    return_code = completed.wait()
    if result is None:
        raise RuntimeError(f"remote build did not report a result; exit code {return_code}")
    return result


def rewrite_local_paths(args: list[str], *, repo_root: Path, remote_root: str) -> list[str]:
    local_root = repo_root.as_posix()
    return [
        _rewrite_local_path(arg, local_root=local_root, remote_root=remote_root) for arg in args
    ]


def remote_path_for(path: Path, *, repo_root: Path, remote_root: str) -> str:
    try:
        relative = path.resolve().relative_to(repo_root.resolve())
    except ValueError as e:
        raise RuntimeError(f"path is outside repo root and cannot be used remotely: {path}") from e
    return f"{remote_root}/{relative.as_posix()}"


def _read_string_tuple(raw: object, *, field_name: str) -> tuple[str, ...]:
    if raw is None:
        return ()
    if not isinstance(raw, list) or not all(isinstance(item, str) for item in raw):
        raise RuntimeError(f"`{field_name}` must be an array of strings")
    return tuple(raw)


def _rewrite_local_path(arg: str, *, local_root: str, remote_root: str) -> str:
    if arg == local_root:
        return remote_root
    if arg.startswith(f"{local_root}/"):
        return f"{remote_root}{arg[len(local_root) :]}"
    name, separator, value = arg.partition("=")
    if separator and value.startswith(f"{local_root}/"):
        return f"{name}={remote_root}{value[len(local_root) :]}"
    return arg


def _remote_build_script(*, configure_cmd: list[str], build_cmd: list[str]) -> str:
    return "\n".join(
        [
            "set -uo pipefail",
            "_time_ms() { python3 -c 'import time; print(time.time_ns() // 1000000)'; }",
            "_configure_started=$(_time_ms)",
            shlex.join(configure_cmd),
            "_configure_exit_code=$?",
            "_configure_finished=$(_time_ms)",
            "_build_exit_code=0",
            "_build_started=$(_time_ms)",
            "if (( _configure_exit_code == 0 )); then",
            f"  {shlex.join(build_cmd)}",
            "  _build_exit_code=$?",
            "fi",
            "_build_finished=$(_time_ms)",
            "printf '%s\\t%s\\t%s\\t%s\\t%s\\n' "
            f"{shlex.quote(_RESULT_MARKER)} "
            '"${_configure_exit_code}" '
            '"${_build_exit_code}" '
            '"$((_configure_finished - _configure_started))" '
            '"$((_build_finished - _build_started))"',
            "if (( _configure_exit_code != 0 )); then",
            "  exit ${_configure_exit_code}",
            "fi",
            "exit ${_build_exit_code}",
            "",
        ]
    )


def _parse_result_line(line: str) -> RemoteBuildResult:
    fields = line.rstrip("\n").split("\t")
    if len(fields) != 5 or fields[0] != _RESULT_MARKER:
        raise RuntimeError(f"malformed remote build result: {line!r}")
    return RemoteBuildResult(
        configure_exit_code=int(fields[1]),
        build_exit_code=int(fields[2]),
        configure_time_ms=int(fields[3]),
        build_time_ms=int(fields[4]),
    )


def _ensure_remote_dir(config: RemoteCompileConfig, path: str) -> None:
    cmd = [
        "ssh",
        *config.ssh_args,
        config.host,
        "mkdir",
        "-p",
        "--",
        path,
    ]
    _ensure_ok(_run(cmd), action="remote directory creation")


def _run(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        text=True,
        capture_output=True,
        check=False,
    )


def _ensure_ok(completed: subprocess.CompletedProcess[str], *, action: str) -> None:
    if completed.returncode == 0:
        return
    stderr = completed.stderr.strip()
    stdout = completed.stdout.strip()
    details = stderr or stdout or f"exit code {completed.returncode}"
    raise RuntimeError(f"{action} failed: {details}")


def _with_trailing_slash(path: str) -> str:
    return path if path.endswith("/") else f"{path}/"
