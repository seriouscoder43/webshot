#!/usr/bin/env python3
from __future__ import annotations

import sys
import urllib.error
import urllib.request


def _check_master_health() -> bool:
    try:
        with urllib.request.urlopen("http://127.0.0.1:9333/cluster/healthz", timeout=1) as response:
            response.read()
        return True
    except (OSError, urllib.error.URLError):
        return False


def _check_s3_endpoint() -> bool:
    try:
        with urllib.request.urlopen("http://127.0.0.1:8333/", timeout=1) as response:
            response.read()
        return True
    except urllib.error.HTTPError as error:
        return 200 <= error.code < 500
    except (OSError, urllib.error.URLError):
        return False


def main() -> int:
    if len(sys.argv) != 1:
        print("usage: python -m s6.check_seaweedfs_ready", file=sys.stderr)
        return 2

    return 0 if _check_master_health() and _check_s3_endpoint() else 1


if __name__ == "__main__":
    raise SystemExit(main())
