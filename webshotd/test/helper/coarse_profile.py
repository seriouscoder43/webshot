from __future__ import annotations

import dataclasses
import datetime as dt
import json
import os
import pathlib
import resource
import time
from contextlib import suppress
from urllib.parse import urlsplit

import pytest
from pytest_userver import client as userver_client
from pytest_userver.plugins import service_client as service_client_plugin

_PROFILE_SCHEMA_VERSION = 2
_PROC_CLK_TCK = os.sysconf("SC_CLK_TCK")
_PROFILE_PATH_NAME = "coarse_profile.json"
_PROFILE_TEXT_PATH_NAME = "coarse_profile.txt"


@dataclasses.dataclass(frozen=True)
class _ProcInfo:
    pid: int
    ppid: int
    start_time_ticks: int
    cpu_ms: int
    args: tuple[str, ...]
    cwd: pathlib.Path | None


@dataclasses.dataclass(frozen=True)
class _ProcessKey:
    pid: int
    start_time_ticks: int


@dataclasses.dataclass(frozen=True)
class _ProcessObservation:
    label: str
    cpu_ms: int


@dataclasses.dataclass(frozen=True)
class _Snapshot:
    wall_ms: int
    python_cpu_user_ms: int
    python_cpu_sys_ms: int
    process_cpu_ms: dict[str, int]


@dataclasses.dataclass
class _TestRecord:
    nodeid: str
    outcome: str = "unknown"
    wall_ms: int = 0
    setup_wall_ms: int = 0
    call_wall_ms: int = 0
    teardown_wall_ms: int = 0
    python_cpu_user_ms: int = 0
    python_cpu_sys_ms: int = 0
    process_cpu_ms: dict[str, int] = dataclasses.field(default_factory=dict)
    capture_job_wall_ms: int = 0
    _counted_capture_job_ids: set[str] = dataclasses.field(default_factory=set)

    def to_json(self) -> dict[str, object]:
        return {
            "nodeid": self.nodeid,
            "outcome": self.outcome,
            "wall_ms": self.wall_ms,
            "setup_wall_ms": self.setup_wall_ms,
            "call_wall_ms": self.call_wall_ms,
            "teardown_wall_ms": self.teardown_wall_ms,
            "python_cpu_user_ms": self.python_cpu_user_ms,
            "python_cpu_sys_ms": self.python_cpu_sys_ms,
            "process_cpu_ms": dict(sorted(self.process_cpu_ms.items())),
            "capture_job_wall_ms": self.capture_job_wall_ms,
        }


def _parse_proc_stat(raw: str) -> tuple[int, int, int, int] | None:
    close_paren = raw.rfind(")")
    if close_paren == -1:
        return None
    pid_text = raw[: raw.find(" ")].strip()
    if not pid_text:
        return None
    parts = raw[close_paren + 2 :].split()
    if len(parts) <= 19:
        return None
    try:
        pid = int(pid_text)
        ppid = int(parts[1])
        utime = int(parts[11])
        stime = int(parts[12])
        start_time_ticks = int(parts[19])
    except ValueError:
        return None
    cpu_ms = ((utime + stime) * 1000) // _PROC_CLK_TCK
    return pid, ppid, start_time_ticks, cpu_ms


def _parse_iso8601_ms(value: str) -> int | None:
    normalized = value
    if normalized.endswith("Z"):
        normalized = normalized[:-1] + "+00:00"
    try:
        parsed = dt.datetime.fromisoformat(normalized)
    except ValueError:
        return None
    return int(parsed.timestamp() * 1000)


def _worker_id(config: pytest.Config) -> str:
    worker_input = getattr(config, "workerinput", None)
    if isinstance(worker_input, dict):
        worker = worker_input.get("workerid")
        if isinstance(worker, str) and worker:
            return worker
    env_worker = os.environ.get("PYTEST_XDIST_WORKER")
    if env_worker:
        return env_worker
    return "master"


def _is_xdist_worker(config: pytest.Config) -> bool:
    return getattr(config, "workerinput", None) is not None


def _pytest_dist_mode(args: list[str]) -> str | None:
    for idx, arg in enumerate(args):
        if arg == "--dist" and idx + 1 < len(args):
            return args[idx + 1]
        if arg.startswith("--dist="):
            return arg.split("=", 1)[1]
    return None


def _int_value(value: object) -> int:
    return value if isinstance(value, int) else 0


def _test_cpu_ms(test: dict[str, object], key: str) -> int:
    return _int_value(test.get(key))


def _test_python_cpu_ms(test: dict[str, object]) -> int:
    return _test_cpu_ms(test, "python_cpu_user_ms") + _test_cpu_ms(test, "python_cpu_sys_ms")


class _ProcessCpuAccumulator:
    def __init__(self) -> None:
        self._current_cpu_ms_by_key: dict[_ProcessKey, int] = {}
        self._label_by_key: dict[_ProcessKey, str] = {}
        self._completed_cpu_ms_by_label: dict[str, int] = {}
        self._completed_cpu_ms_by_key: dict[_ProcessKey, int] = {}
        self._completed_label_by_key: dict[_ProcessKey, str] = {}

    def snapshot_cpu_ms(
        self, observations: dict[_ProcessKey, _ProcessObservation]
    ) -> dict[str, int]:
        missing_keys = set(self._current_cpu_ms_by_key) - set(observations)
        for key in missing_keys:
            self._complete_process(key)

        for key, observation in observations.items():
            self._restore_completed_process(key)
            previous_label = self._label_by_key.get(key)
            if previous_label is not None and previous_label != observation.label:
                self._complete_process(key)

            previous_cpu_ms = self._current_cpu_ms_by_key.get(key, 0)
            self._current_cpu_ms_by_key[key] = max(previous_cpu_ms, observation.cpu_ms)
            self._label_by_key[key] = observation.label

        result = dict(self._completed_cpu_ms_by_label)
        for key, cpu_ms in self._current_cpu_ms_by_key.items():
            label = self._label_by_key[key]
            result[label] = result.get(label, 0) + cpu_ms
        return {label: cpu_ms for label, cpu_ms in sorted(result.items()) if cpu_ms > 0}

    def _complete_process(self, key: _ProcessKey) -> None:
        cpu_ms = self._current_cpu_ms_by_key.pop(key, 0)
        label = self._label_by_key.pop(key, None)
        if label is None or cpu_ms <= 0:
            return
        self._completed_cpu_ms_by_label[label] = (
            self._completed_cpu_ms_by_label.get(label, 0) + cpu_ms
        )
        self._completed_cpu_ms_by_key[key] = cpu_ms
        self._completed_label_by_key[key] = label

    def _restore_completed_process(self, key: _ProcessKey) -> None:
        cpu_ms = self._completed_cpu_ms_by_key.pop(key, None)
        label = self._completed_label_by_key.pop(key, None)
        if cpu_ms is None or label is None:
            return
        remaining_cpu_ms = self._completed_cpu_ms_by_label.get(label, 0) - cpu_ms
        if remaining_cpu_ms > 0:
            self._completed_cpu_ms_by_label[label] = remaining_cpu_ms
        else:
            self._completed_cpu_ms_by_label.pop(label, None)
        self._current_cpu_ms_by_key[key] = cpu_ms
        self._label_by_key[key] = label


class _CoarseProfiler:
    def __init__(self, config: pytest.Config) -> None:
        self._config = config
        service_binary = getattr(config.option, "service_binary", None)
        self._service_binary = pathlib.Path(service_binary).resolve() if service_binary else None
        self._service_binary_candidates = self._build_service_binary_candidates(
            self._service_binary
        )
        basetemp_option = getattr(config.option, "basetemp", None)
        basetemp = (
            pathlib.Path(str(basetemp_option)).resolve() if basetemp_option else pathlib.Path.cwd()
        )
        self.worker_id = _worker_id(config)
        self._is_worker = _is_xdist_worker(config)
        self._own_pid = os.getpid()
        self._has_worker_tmp_dir = basetemp_option is not None
        self._worker_tmp_dir = basetemp
        self._service_config_path = basetemp / "webshotd_wrapper" / "config.yaml"
        self.output_path = basetemp / _PROFILE_PATH_NAME
        self.summary_path = basetemp / _PROFILE_TEXT_PATH_NAME
        self._errors: list[str] = []
        self._error_keys: set[str] = set()
        self._test_records: dict[str, _TestRecord] = {}
        self._test_order: list[str] = []
        self._test_start_snapshots: dict[str, _Snapshot] = {}
        self._current_test_nodeid: str | None = None
        self._job_owner_by_id: dict[str, str] = {}
        self._session_start_snapshot: _Snapshot | None = None
        self._session_end_snapshot: _Snapshot | None = None
        self._session_started_at = dt.datetime.now(dt.UTC)
        self._session_finished_at: dt.datetime | None = None
        self._process_cpu = _ProcessCpuAccumulator()

    def start_session(self) -> None:
        self._session_start_snapshot = self._snapshot()

    def finish_session(self) -> None:
        self._session_end_snapshot = self._snapshot()
        self._session_finished_at = dt.datetime.now(dt.UTC)
        summary_written = False
        try:
            self.output_path.parent.mkdir(parents=True, exist_ok=True)
            self.summary_path.write_text(self._build_summary_text(), encoding="utf-8")
            summary_written = True
        except OSError as exc:
            self.record_error_once(
                "write_text_failed",
                f"coarse_profile: failed to write {self.summary_path}: {exc}",
            )

        payload = self._build_json_payload()
        try:
            self.output_path.write_text(
                json.dumps(payload, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
        except OSError as exc:
            self.record_error_once(
                "write_json_failed",
                f"coarse_profile: failed to write {self.output_path}: {exc}",
            )
        else:
            if summary_written and self._errors:
                with suppress(OSError):
                    self.summary_path.write_text(self._build_summary_text(), encoding="utf-8")
        if not self._is_worker:
            self._write_aggregate_profile(payload)

    def start_test(self, nodeid: str) -> None:
        self._current_test_nodeid = nodeid
        if nodeid not in self._test_records:
            self._test_records[nodeid] = _TestRecord(nodeid=nodeid)
            self._test_order.append(nodeid)
        self._test_start_snapshots[nodeid] = self._snapshot()

    def finish_test(self, nodeid: str) -> None:
        start_snapshot = self._test_start_snapshots.pop(nodeid, None)
        end_snapshot = self._snapshot()
        if start_snapshot is None:
            self._current_test_nodeid = None
            return

        record = self._test_records[nodeid]
        delta = self._diff_snapshots(start_snapshot, end_snapshot)
        record.wall_ms = delta.wall_ms
        record.python_cpu_user_ms = delta.python_cpu_user_ms
        record.python_cpu_sys_ms = delta.python_cpu_sys_ms
        record.process_cpu_ms = dict(delta.process_cpu_ms)
        self._current_test_nodeid = None

    def record_test_outcome(self, report: pytest.TestReport) -> None:
        record = self._test_records.get(report.nodeid)
        if record is None:
            return
        duration_ms = max(0, int(report.duration * 1000))
        if report.when == "setup":
            record.setup_wall_ms = duration_ms
        elif report.when == "call":
            record.call_wall_ms = duration_ms
        elif report.when == "teardown":
            record.teardown_wall_ms = duration_ms
        if report.when == "call":
            record.outcome = report.outcome
            return
        if report.failed:
            record.outcome = "failed"
            return
        if report.skipped and record.outcome == "unknown":
            record.outcome = "skipped"

    def record_capture_post(self, payload: object) -> None:
        if not isinstance(payload, dict):
            return
        job_id = payload.get("uuid")
        if not isinstance(job_id, str) or not job_id:
            return
        owner = self._current_test_nodeid
        if owner is None:
            return
        self._job_owner_by_id[job_id] = owner

    def record_capture_status(self, job_id: str, payload: object) -> None:
        if not isinstance(payload, dict):
            return
        owner = self._job_owner_by_id.get(job_id)
        if owner is None:
            return
        record = self._test_records.get(owner)
        if record is None or job_id in record._counted_capture_job_ids:
            return
        status = payload.get("status")
        if status not in {"succeeded", "failed"}:
            return

        started_at = payload.get("started_at")
        finished_at = payload.get("finished_at")
        if not isinstance(started_at, str) or not isinstance(finished_at, str):
            return
        started_at_ms = _parse_iso8601_ms(started_at)
        finished_at_ms = _parse_iso8601_ms(finished_at)
        if started_at_ms is None or finished_at_ms is None:
            self.record_error_once(
                f"bad_job_timestamps_{job_id}",
                f"coarse_profile: failed to parse capture timestamps for job {job_id}",
            )
            return
        if finished_at_ms < started_at_ms:
            self.record_error_once(
                f"inverted_job_timestamps_{job_id}",
                f"coarse_profile: capture finished_at < started_at for job {job_id}",
            )
            return

        record.capture_job_wall_ms += finished_at_ms - started_at_ms
        record._counted_capture_job_ids.add(job_id)

    def record_error_once(self, key: str, message: str) -> None:
        if key in self._error_keys:
            return
        self._error_keys.add(key)
        self._errors.append(message)

    def _build_summary_lines(self) -> list[str]:
        if self._session_start_snapshot is None or self._session_end_snapshot is None:
            return []

        session_summary = self._session_summary()
        lines = [f"json: {self.output_path}"]
        python_cpu_ms = session_summary["python_cpu_user_ms"] + session_summary["python_cpu_sys_ms"]
        lines.append(
            "session: "
            f"wall={session_summary['wall_ms']} ms "
            f"python={python_cpu_ms} ms "
            f"processes={self._sum_process_cpu_ms(session_summary.get('process_cpu_ms'))} ms "
            f"capture_jobs={session_summary['capture_job_wall_ms']} ms"
        )
        process_line = self._format_processes_line(session_summary.get("process_cpu_ms"))
        if process_line is not None:
            lines.append(process_line)

        for title, key in (
            ("top wall", "wall_ms"),
            ("top process CPU", "_process_cpu_ms"),
            ("top python", "_python_cpu_ms"),
        ):
            lines.extend(self._format_top_lines(title=title, key=key))

        if self._errors:
            lines.append("warnings:")
            for message in self._errors:
                lines.append(f"  {message}")

        return lines

    def _build_summary_text(self) -> str:
        lines = self._build_summary_lines()
        return "\n".join(lines) + "\n" if lines else ""

    def _format_top_lines(self, *, title: str, key: str) -> list[str]:
        ranked: list[tuple[int, _TestRecord]] = []
        for nodeid in self._test_order:
            record = self._test_records[nodeid]
            if key == "_python_cpu_ms":
                value = record.python_cpu_user_ms + record.python_cpu_sys_ms
            elif key == "_process_cpu_ms":
                value = self._sum_process_cpu_ms(record.process_cpu_ms)
            else:
                value = getattr(record, key)
            if value <= 0:
                continue
            ranked.append((value, record))
        ranked.sort(key=lambda item: item[0], reverse=True)
        if not ranked:
            return [f"{title}: none"]

        formatted = [f"{title}:"]
        for value, record in ranked[:5]:
            formatted.append(f"  {value} ms  {record.nodeid} ({record.outcome})")
        return formatted

    @staticmethod
    def _sum_process_cpu_ms(process_cpu_ms: object) -> int:
        if not isinstance(process_cpu_ms, dict):
            return 0
        return sum(value for value in process_cpu_ms.values() if isinstance(value, int))

    @staticmethod
    def _format_processes_line(process_cpu_ms: object) -> str | None:
        if not isinstance(process_cpu_ms, dict):
            return None
        items = [
            (str(name), value)
            for name, value in process_cpu_ms.items()
            if isinstance(value, int) and value > 0
        ]
        if not items:
            return None
        items.sort(key=lambda item: (-item[1], item[0]))
        rendered = ", ".join(f"{name}={value} ms" for name, value in items)
        return f"processes: {rendered}"

    def _build_json_payload(self) -> dict[str, object]:
        return {
            "meta": {
                "schema_version": _PROFILE_SCHEMA_VERSION,
                "aggregate": False,
                "worker_id": self.worker_id,
                "xdist_worker_count": os.environ.get("PYTEST_XDIST_WORKER_COUNT"),
                "xdist_scheduler": _pytest_dist_mode(list(self._config.invocation_params.args)),
                "pytest_argv": list(self._config.invocation_params.args),
                "testsuite_working_dir": str(pathlib.Path.cwd()),
                "started_at": self._session_started_at.isoformat(),
                "finished_at": (
                    self._session_finished_at.isoformat()
                    if self._session_finished_at is not None
                    else None
                ),
                "output_path": str(self.output_path),
                "summary_path": str(self.summary_path),
            },
            "profiling_errors": list(self._errors),
            "session": self._session_summary(),
            "tests": [self._test_records[nodeid].to_json() for nodeid in self._test_order],
        }

    def _session_summary(self) -> dict[str, object]:
        if self._session_start_snapshot is None or self._session_end_snapshot is None:
            return {
                "test_count": len(self._test_order),
                "wall_ms": 0,
                "python_cpu_user_ms": 0,
                "python_cpu_sys_ms": 0,
                "process_cpu_ms": {},
                "capture_job_wall_ms": 0,
            }

        delta = self._diff_snapshots(self._session_start_snapshot, self._session_end_snapshot)
        return {
            "test_count": len(self._test_order),
            "wall_ms": delta.wall_ms,
            "python_cpu_user_ms": delta.python_cpu_user_ms,
            "python_cpu_sys_ms": delta.python_cpu_sys_ms,
            "process_cpu_ms": dict(sorted(delta.process_cpu_ms.items())),
            "capture_job_wall_ms": sum(
                self._test_records[nodeid].capture_job_wall_ms for nodeid in self._test_order
            ),
        }

    def _write_aggregate_profile(self, master_payload: dict[str, object]) -> None:
        worker_payloads = self._read_worker_payloads()
        if not worker_payloads:
            return

        aggregate = self._build_aggregate_payload(master_payload, worker_payloads)
        try:
            self.output_path.write_text(
                json.dumps(aggregate, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            self.summary_path.write_text(
                self._build_aggregate_summary_text(aggregate), encoding="utf-8"
            )
        except OSError as exc:
            self.record_error_once(
                "write_aggregate_failed",
                f"coarse_profile: failed to write aggregate profile: {exc}",
            )

    def _read_worker_payloads(self) -> list[dict[str, object]]:
        payloads: list[dict[str, object]] = []
        for path in sorted(self.output_path.parent.glob("popen-gw*/" + _PROFILE_PATH_NAME)):
            try:
                payload = json.loads(path.read_text(encoding="utf-8"))
            except (OSError, ValueError) as exc:
                self.record_error_once(
                    f"read_worker_profile_failed_{path.parent.name}",
                    f"coarse_profile: failed to read worker profile {path}: {exc}",
                )
                continue
            if isinstance(payload, dict):
                payloads.append(payload)
        return payloads

    def _build_aggregate_payload(
        self, master_payload: dict[str, object], worker_payloads: list[dict[str, object]]
    ) -> dict[str, object]:
        tests: list[dict[str, object]] = []
        workers: list[dict[str, object]] = []
        profiling_errors = list(self._errors)

        for index, payload in enumerate(worker_payloads):
            meta = payload.get("meta")
            if not isinstance(meta, dict):
                meta = {}
            worker = meta.get("worker_id")
            worker_id = worker if isinstance(worker, str) and worker else f"worker-{index}"
            session = payload.get("session")
            if not isinstance(session, dict):
                session = {}
            worker_tests = payload.get("tests")
            if not isinstance(worker_tests, list):
                worker_tests = []

            workers.append(
                {
                    "worker_id": worker_id,
                    "output_path": meta.get("output_path"),
                    "summary_path": meta.get("summary_path"),
                    "session": session,
                    "test_count": len(worker_tests),
                }
            )
            for test in worker_tests:
                if not isinstance(test, dict):
                    continue
                item = dict(test)
                item["worker_id"] = worker_id
                tests.append(item)

            errors = payload.get("profiling_errors")
            if isinstance(errors, list):
                for error in errors:
                    if isinstance(error, str):
                        profiling_errors.append(f"{worker_id}: {error}")

        master_session = master_payload.get("session")
        if not isinstance(master_session, dict):
            master_session = {}
        session = self._aggregate_session(master_session, worker_payloads, tests)
        meta = {
            "schema_version": _PROFILE_SCHEMA_VERSION,
            "aggregate": True,
            "worker_id": self.worker_id,
            "worker_count": len(workers),
            "xdist_worker_count": os.environ.get("PYTEST_XDIST_WORKER_COUNT"),
            "xdist_scheduler": _pytest_dist_mode(list(self._config.invocation_params.args)),
            "pytest_argv": list(self._config.invocation_params.args),
            "testsuite_working_dir": str(pathlib.Path.cwd()),
            "started_at": self._session_started_at.isoformat(),
            "finished_at": (
                self._session_finished_at.isoformat()
                if self._session_finished_at is not None
                else None
            ),
            "output_path": str(self.output_path),
            "summary_path": str(self.summary_path),
        }
        return {
            "meta": meta,
            "profiling_errors": profiling_errors,
            "session": session,
            "workers": workers,
            "tests": tests,
        }

    @staticmethod
    def _aggregate_session(
        master_session: dict[str, object],
        worker_payloads: list[dict[str, object]],
        tests: list[dict[str, object]],
    ) -> dict[str, object]:
        process_cpu_ms: dict[str, int] = {}
        python_cpu_user_ms = 0
        python_cpu_sys_ms = 0
        capture_job_wall_ms = 0
        worker_wall_ms = 0

        for payload in worker_payloads:
            session = payload.get("session")
            if not isinstance(session, dict):
                continue
            worker_wall_ms = max(worker_wall_ms, _int_value(session.get("wall_ms")))
            python_cpu_user_ms += _int_value(session.get("python_cpu_user_ms"))
            python_cpu_sys_ms += _int_value(session.get("python_cpu_sys_ms"))
            capture_job_wall_ms += _int_value(session.get("capture_job_wall_ms"))
            worker_processes = session.get("process_cpu_ms")
            if isinstance(worker_processes, dict):
                for label, value in worker_processes.items():
                    if not isinstance(label, str):
                        continue
                    process_cpu_ms[label] = process_cpu_ms.get(label, 0) + _int_value(value)

        return {
            "test_count": len(tests),
            "wall_ms": max(_int_value(master_session.get("wall_ms")), worker_wall_ms),
            "python_cpu_user_ms": python_cpu_user_ms,
            "python_cpu_sys_ms": python_cpu_sys_ms,
            "process_cpu_ms": dict(sorted(process_cpu_ms.items())),
            "capture_job_wall_ms": capture_job_wall_ms,
        }

    @staticmethod
    def _build_aggregate_summary_text(payload: dict[str, object]) -> str:
        meta = payload.get("meta")
        session = payload.get("session")
        tests = payload.get("tests")
        workers = payload.get("workers")
        errors = payload.get("profiling_errors")
        if not isinstance(meta, dict):
            meta = {}
        if not isinstance(session, dict):
            session = {}
        if not isinstance(tests, list):
            tests = []
        if not isinstance(workers, list):
            workers = []
        if not isinstance(errors, list):
            errors = []

        python_cpu_ms = _int_value(session.get("python_cpu_user_ms")) + _int_value(
            session.get("python_cpu_sys_ms")
        )
        lines = [
            f"json: {meta.get('output_path')}",
            "aggregate: "
            f"workers={len(workers)} "
            f"tests={_int_value(session.get('test_count'))} "
            f"scheduler={meta.get('xdist_scheduler')}",
            "session: "
            f"wall={_int_value(session.get('wall_ms'))} ms "
            f"python={python_cpu_ms} ms "
            f"processes={_CoarseProfiler._sum_process_cpu_ms(session.get('process_cpu_ms'))} ms "
            f"capture_jobs={_int_value(session.get('capture_job_wall_ms'))} ms",
        ]
        process_line = _CoarseProfiler._format_processes_line(session.get("process_cpu_ms"))
        if process_line is not None:
            lines.append(process_line)

        lines.extend(_CoarseProfiler._format_worker_top_lines(workers))
        for title, key in (
            ("top wall", "wall_ms"),
            ("top setup", "setup_wall_ms"),
            ("top call", "call_wall_ms"),
            ("top teardown", "teardown_wall_ms"),
            ("top capture jobs", "capture_job_wall_ms"),
            ("top process CPU", "_process_cpu_ms"),
            ("top python", "_python_cpu_ms"),
        ):
            lines.extend(_CoarseProfiler._format_json_top_lines(tests, title=title, key=key))

        clean_errors = [error for error in errors if isinstance(error, str)]
        if clean_errors:
            lines.append("warnings:")
            for message in clean_errors:
                lines.append(f"  {message}")
        return "\n".join(lines) + "\n"

    @staticmethod
    def _format_worker_top_lines(workers: list[object]) -> list[str]:
        ranked: list[tuple[int, str, int]] = []
        for worker in workers:
            if not isinstance(worker, dict):
                continue
            session = worker.get("session")
            if not isinstance(session, dict):
                continue
            worker_id = worker.get("worker_id")
            name = worker_id if isinstance(worker_id, str) else "unknown"
            ranked.append(
                (_int_value(session.get("wall_ms")), name, _int_value(worker.get("test_count")))
            )
        ranked.sort(reverse=True)
        if not ranked:
            return ["slow workers: none"]
        lines = ["slow workers:"]
        for wall_ms, worker_id, test_count in ranked[:8]:
            lines.append(f"  {wall_ms} ms  {worker_id} tests={test_count}")
        return lines

    @staticmethod
    def _format_json_top_lines(tests: list[object], *, title: str, key: str) -> list[str]:
        ranked: list[tuple[int, dict[str, object]]] = []
        for test in tests:
            if not isinstance(test, dict):
                continue
            if key == "_python_cpu_ms":
                value = _test_python_cpu_ms(test)
            elif key == "_process_cpu_ms":
                value = _CoarseProfiler._sum_process_cpu_ms(test.get("process_cpu_ms"))
            else:
                value = _int_value(test.get(key))
            if value <= 0:
                continue
            ranked.append((value, test))
        ranked.sort(key=lambda item: item[0], reverse=True)
        if not ranked:
            return [f"{title}: none"]

        lines = [f"{title}:"]
        for value, test in ranked[:8]:
            worker = test.get("worker_id")
            worker_text = worker if isinstance(worker, str) else "unknown"
            nodeid = test.get("nodeid")
            nodeid_text = nodeid if isinstance(nodeid, str) else "<unknown>"
            outcome = test.get("outcome")
            outcome_text = outcome if isinstance(outcome, str) else "unknown"
            lines.append(f"  {value} ms  {worker_text}  {nodeid_text} ({outcome_text})")
        return lines

    def _snapshot(self) -> _Snapshot:
        proc_table = self._read_proc_table()
        children_by_ppid: dict[int, list[int]] = {}
        for proc in proc_table.values():
            children_by_ppid.setdefault(proc.ppid, []).append(proc.pid)

        process_cpu_ms = self._process_cpu.snapshot_cpu_ms(
            self._worker_process_observations(proc_table, children_by_ppid)
        )

        usage = resource.getrusage(resource.RUSAGE_SELF)
        return _Snapshot(
            wall_ms=time.monotonic_ns() // 1_000_000,
            python_cpu_user_ms=int(usage.ru_utime * 1000),
            python_cpu_sys_ms=int(usage.ru_stime * 1000),
            process_cpu_ms=process_cpu_ms,
        )

    def _worker_process_observations(
        self,
        proc_table: dict[int, _ProcInfo],
        children_by_ppid: dict[int, list[int]],
    ) -> dict[_ProcessKey, _ProcessObservation]:
        scoped_pids = self._worker_scoped_pids(proc_table, children_by_ppid)
        observations: dict[_ProcessKey, _ProcessObservation] = {}
        for pid in scoped_pids:
            if pid == self._own_pid:
                continue
            proc = proc_table.get(pid)
            if proc is None:
                continue
            key = _ProcessKey(pid=proc.pid, start_time_ticks=proc.start_time_ticks)
            observations[key] = _ProcessObservation(
                label=self._process_label(proc),
                cpu_ms=proc.cpu_ms,
            )
        return observations

    def _worker_scoped_pids(
        self,
        proc_table: dict[int, _ProcInfo],
        children_by_ppid: dict[int, list[int]],
    ) -> set[int]:
        root_pids = self._worker_root_pids(proc_table)
        scoped_pids: set[int] = set()
        pending = list(root_pids)
        while pending:
            pid = pending.pop()
            if pid in scoped_pids:
                continue
            if pid not in proc_table:
                continue
            scoped_pids.add(pid)
            pending.extend(children_by_ppid.get(pid, ()))
        return scoped_pids

    def _worker_root_pids(self, proc_table: dict[int, _ProcInfo]) -> set[int]:
        service_matches = [
            proc for proc in proc_table.values() if self._is_service_binary_proc(proc)
        ]
        roots = {proc.pid for proc in service_matches if self._proc_references_worker_tmp(proc)}

        tmp_reference_roots = {
            proc.pid for proc in proc_table.values() if self._proc_references_worker_tmp(proc)
        }
        roots.update(tmp_reference_roots)

        if roots:
            service_roots = sorted(proc.pid for proc in service_matches if proc.pid in roots)
            if len(service_roots) > 1:
                self.record_error_once(
                    "multiple_worker_service_pids",
                    "coarse_profile: multiple service PIDs reference this worker: "
                    f"{service_roots!r}",
                )
            return roots

        if len(service_matches) == 1:
            return {service_matches[0].pid}
        if len(service_matches) > 1:
            self.record_error_once(
                "multiple_unscoped_service_pids",
                "coarse_profile: multiple service PIDs exist but none references this worker: "
                f"{sorted(proc.pid for proc in service_matches)!r}",
            )
        return set()

    def _is_service_binary_proc(self, proc: _ProcInfo) -> bool:
        if not self._service_binary_candidates or not proc.args:
            return False
        try:
            binary = pathlib.Path(proc.args[0]).resolve()
        except OSError:
            return False
        return binary in self._service_binary_candidates

    @staticmethod
    def _process_label(proc: _ProcInfo) -> str:
        if not proc.args:
            return f"pid:{proc.pid}"
        name = pathlib.Path(proc.args[0]).name
        return name or f"pid:{proc.pid}"

    def _read_proc_table(self) -> dict[int, _ProcInfo]:
        proc_table: dict[int, _ProcInfo] = {}
        proc_root = pathlib.Path("/proc")
        for entry in proc_root.iterdir():
            if not entry.name.isdigit():
                continue
            stat_path = entry / "stat"
            cmdline_path = entry / "cmdline"
            try:
                stat_raw = stat_path.read_text(encoding="utf-8")
                cmdline_raw = cmdline_path.read_bytes()
            except OSError:
                continue

            parsed_stat = _parse_proc_stat(stat_raw)
            if parsed_stat is None:
                continue
            pid, ppid, start_time_ticks, cpu_ms = parsed_stat
            args = tuple(
                part.decode("utf-8", "replace") for part in cmdline_raw.split(b"\0") if part
            )
            try:
                cwd = (entry / "cwd").resolve()
            except OSError:
                cwd = None
            proc_table[pid] = _ProcInfo(
                pid=pid,
                ppid=ppid,
                start_time_ticks=start_time_ticks,
                cpu_ms=cpu_ms,
                args=args,
                cwd=cwd,
            )
        return proc_table

    def _proc_references_worker_tmp(self, proc: _ProcInfo) -> bool:
        config_path = str(self._service_config_path)
        if any(config_path in arg for arg in proc.args):
            return True
        if not self._has_worker_tmp_dir:
            return False
        tmp_dir = str(self._worker_tmp_dir)
        if any(tmp_dir in arg for arg in proc.args):
            return True
        return proc.cwd is not None and (
            proc.cwd == self._worker_tmp_dir or self._worker_tmp_dir in proc.cwd.parents
        )

    @staticmethod
    def _build_service_binary_candidates(
        service_binary: pathlib.Path | None,
    ) -> set[pathlib.Path]:
        if service_binary is None:
            return set()

        candidates = {service_binary}
        sibling_binary = service_binary.parent / "webshotd"
        if sibling_binary.exists():
            candidates.add(sibling_binary.resolve())
        return candidates

    @staticmethod
    def _diff_snapshots(start: _Snapshot, end: _Snapshot) -> _Snapshot:
        process_labels = set(start.process_cpu_ms) | set(end.process_cpu_ms)
        return _Snapshot(
            wall_ms=max(0, end.wall_ms - start.wall_ms),
            python_cpu_user_ms=max(0, end.python_cpu_user_ms - start.python_cpu_user_ms),
            python_cpu_sys_ms=max(0, end.python_cpu_sys_ms - start.python_cpu_sys_ms),
            process_cpu_ms={
                label: max(0, end.process_cpu_ms.get(label, 0) - start.process_cpu_ms.get(label, 0))
                for label in sorted(process_labels)
            },
        )


class _ProfiledServiceClient:
    def __init__(self, client, profiler: _CoarseProfiler) -> None:
        self._client = client
        self._profiler = profiler

    def __getattr__(self, name: str):
        return getattr(self._client, name)

    async def get(self, path: str, **kwargs):
        response = await self._client.get(path, **kwargs)
        self._record_response("GET", path, response)
        return response

    async def post(self, path: str, **kwargs):
        response = await self._client.post(path, **kwargs)
        self._record_response("POST", path, response)
        return response

    def _record_response(self, method: str, path: str, response) -> None:
        try:
            normalized_path = urlsplit(path).path
            if method == "POST" and normalized_path == "/v1/capture" and response.status == 202:
                self._profiler.record_capture_post(response.json())
                return
            if (
                method == "GET"
                and normalized_path.startswith("/v1/capture/jobs/")
                and response.status == 200
            ):
                job_id = normalized_path.rsplit("/", 1)[-1]
                if job_id:
                    self._profiler.record_capture_status(job_id, response.json())
        except Exception as exc:
            self._profiler.record_error_once(
                f"response_parse_failed_{method}_{path}",
                f"coarse_profile: failed to inspect {method} {path}: {exc}",
            )


def _require_profiler(config: pytest.Config) -> _CoarseProfiler:
    profiler = getattr(config, "_webshot_coarse_profiler", None)
    if profiler is None:
        raise RuntimeError("missing _webshot_coarse_profiler on pytest config")
    return profiler


def pytest_configure(config: pytest.Config) -> None:
    config._webshot_coarse_profiler = _CoarseProfiler(config)


def pytest_sessionstart(session: pytest.Session) -> None:
    _require_profiler(session.config).start_session()


def pytest_sessionfinish(session: pytest.Session, exitstatus: int) -> None:
    _require_profiler(session.config).finish_session()


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_protocol(item: pytest.Item, nextitem):
    profiler = _require_profiler(item.config)
    profiler.start_test(item.nodeid)
    yield
    profiler.finish_test(item.nodeid)


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item: pytest.Item, call: pytest.CallInfo[None]):
    outcome = yield
    report = outcome.get_result()
    _require_profiler(item.config).record_test_outcome(report)


def pytest_terminal_summary(terminalreporter: pytest.TerminalReporter) -> None:
    del terminalreporter


@pytest.fixture
async def service_client(
    request: pytest.FixtureRequest,
    service_daemon_instance,
    service_baseurl,
    service_client_options,
    userver_service_client_options,
    userver_client_cleanup,
    _testsuite_client_config: userver_client.TestsuiteClientConfig,
):
    profiler = _require_profiler(request.config)
    if not _testsuite_client_config.testsuite_action_path:
        yield _ProfiledServiceClient(
            service_client_plugin._ClientDiagnose(service_baseurl, **service_client_options),
            profiler,
        )
        return

    aiohttp_client = userver_client.AiohttpClient(
        service_baseurl,
        **userver_service_client_options,
    )
    profiled_client = userver_client.Client(aiohttp_client)
    async with userver_client_cleanup(profiled_client):
        yield _ProfiledServiceClient(profiled_client, profiler)
