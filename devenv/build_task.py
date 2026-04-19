#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def _repo_root() -> Path:
    root = Path(__file__).resolve().parents[1]
    if not root.joinpath("devenv.nix").is_file():
        raise RuntimeError(f"failed to locate repo root from {__file__}")
    return root


def _timestamp_utc() -> str:
    return dt.datetime.now(dt.UTC).strftime("%Y-%m-%dT%H:%M:%SZ")


def _time_ms() -> int:
    return time.time_ns() // 1_000_000


def _parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="build_task")
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--clangd-file", required=True)
    parser.add_argument("--configure-fingerprint", required=True)
    parser.add_argument("--configure-arg", action="append", default=[])
    parser.add_argument("--build-arg", action="append", default=[])
    parser.add_argument("--timings-output")
    parser.add_argument("--timings-collector")
    args = parser.parse_args(argv)
    if args.timings_output and not args.timings_collector:
        raise RuntimeError("--timings-collector is required when --timings-output is set")
    return args


def _build_dir_has_configure_state(build_dir: Path) -> bool:
    return (
        build_dir.joinpath("CMakeCache.txt").is_file() or build_dir.joinpath("CMakeFiles").is_dir()
    )


def _should_configure_fresh(build_dir: Path, configure_fingerprint: str) -> bool:
    fingerprint_path = build_dir / ".configure-fingerprint"
    if fingerprint_path.is_file():
        previous_fingerprint = fingerprint_path.read_text(encoding="utf-8").strip()
        if previous_fingerprint != configure_fingerprint:
            print("CMake configure spec changed; reconfiguring with --fresh")
            return True
        return False

    if _build_dir_has_configure_state(build_dir):
        print("Existing build dir has no configure fingerprint; reconfiguring with --fresh")
        return True

    return False


def _configure_command(configure_argv: list[str], *, fresh: bool) -> list[str]:
    if not configure_argv:
        raise RuntimeError("missing configure argv")
    if configure_argv[0] != "cmake":
        raise RuntimeError(
            f"expected configure argv to start with cmake, got: {configure_argv[0]!r}"
        )
    if not fresh:
        return configure_argv
    return [configure_argv[0], "--fresh", *configure_argv[1:]]


def _run(cmd: list[str], *, cwd: Path) -> int:
    completed = subprocess.run(cmd, cwd=cwd, check=False)
    return completed.returncode


def _write_clangd_symlink(repo_root: Path, clangd_file: Path) -> None:
    target = repo_root / ".clangd"
    target.unlink(missing_ok=True)
    target.symlink_to(clangd_file)


def _snapshot_ninja_log(build_dir: Path) -> Path:
    with tempfile.NamedTemporaryFile(
        prefix="webshot_build_times_before.",
        dir="/tmp",
        delete=False,
    ) as handle:
        snapshot_path = Path(handle.name)

    ninja_log = build_dir / ".ninja_log"
    if ninja_log.is_file():
        snapshot_path.write_bytes(ninja_log.read_bytes())
    else:
        snapshot_path.write_text("", encoding="utf-8")
    return snapshot_path


def _collect_timings(
    *,
    repo_root: Path,
    collector: Path,
    build_dir: Path,
    before_log: Path,
    output: Path,
    status: str,
    started_at: str,
    finished_at: str,
    wall_time_ms: int,
    configure_time_ms: int,
    build_time_ms: int,
) -> int:
    return _run(
        [
            sys.executable,
            str(collector),
            "--build-dir",
            str(build_dir),
            "--before-log",
            str(before_log),
            "--after-log",
            str(build_dir / ".ninja_log"),
            "--output",
            str(output),
            "--status",
            status,
            "--started-at",
            started_at,
            "--finished-at",
            finished_at,
            "--wall-time-ms",
            str(wall_time_ms),
            "--configure-time-ms",
            str(configure_time_ms),
            "--build-time-ms",
            str(build_time_ms),
        ],
        cwd=repo_root,
    )


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv)
    repo_root = _repo_root()
    build_dir = Path(args.build_dir)
    configure_fingerprint_path = build_dir / ".configure-fingerprint"
    configure_fresh = _should_configure_fresh(build_dir, args.configure_fingerprint)

    before_log_path: Path | None = None
    started_at = _timestamp_utc()
    started_ms = _time_ms()
    if args.timings_output:
        before_log_path = _snapshot_ninja_log(build_dir)

    configure_started_ms = _time_ms()
    configure_exit_code = _run(
        _configure_command(args.configure_arg, fresh=configure_fresh),
        cwd=repo_root,
    )
    configure_finished_ms = _time_ms()
    configure_time_ms = configure_finished_ms - configure_started_ms

    if configure_exit_code == 0:
        configure_fingerprint_path.write_text(args.configure_fingerprint + "\n", encoding="utf-8")
        _write_clangd_symlink(repo_root, Path(args.clangd_file))

    build_started_ms = _time_ms()
    build_exit_code = 0
    if configure_exit_code == 0:
        build_exit_code = _run(
            ["cmake", "--build", str(build_dir), *args.build_arg],
            cwd=repo_root,
        )
    build_finished_ms = _time_ms()
    build_time_ms = build_finished_ms - build_started_ms

    exit_code = configure_exit_code if configure_exit_code != 0 else build_exit_code

    if args.timings_output:
        assert before_log_path is not None
        finished_at = _timestamp_utc()
        finished_ms = _time_ms()
        wall_time_ms = finished_ms - started_ms
        status = "success" if exit_code == 0 else "failure"
        try:
            collect_exit_code = _collect_timings(
                repo_root=repo_root,
                collector=Path(args.timings_collector),
                build_dir=build_dir,
                before_log=before_log_path,
                output=Path(args.timings_output),
                status=status,
                started_at=started_at,
                finished_at=finished_at,
                wall_time_ms=wall_time_ms,
                configure_time_ms=configure_time_ms,
                build_time_ms=build_time_ms,
            )
        finally:
            before_log_path.unlink(missing_ok=True)
        if collect_exit_code != 0:
            return collect_exit_code

    return exit_code


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as e:
        print(f"build_task: {e}", file=sys.stderr)
        raise SystemExit(1) from None
