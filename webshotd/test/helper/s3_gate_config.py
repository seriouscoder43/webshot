_S3_GATE_HOST = "127.0.0.1"
_S3_GATE_TIMEOUT_MS = 1000
_s3_gate_port: int | None = None


def set_s3_gate_port(port: int) -> None:
    global _s3_gate_port
    if port <= 0:
        raise RuntimeError(f"invalid S3 gate port: {port}")
    _s3_gate_port = port


def enable_s3_gate(config_yaml, _config_vars) -> None:
    if _s3_gate_port is None:
        raise RuntimeError("S3 gate port must be selected before enabling S3 gate config")

    components = config_yaml["components_manager"]["components"]
    cfg = components["config"]
    cfg["s3_endpoint"] = f"http://{_S3_GATE_HOST}:{_s3_gate_port}"
    cfg["s3_timeout_ms"] = _S3_GATE_TIMEOUT_MS

    http_client_core = components["http-client-core"]
    if http_client_core is None:
        raise RuntimeError("http-client-core component config must be present")
    http_client_core["testsuite-timeout"] = "1s"
