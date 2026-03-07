#!/usr/bin/env python3

import json
import sys


def main() -> None:
    raw = sys.stdin.read()
    if not raw.strip():
        sys.stderr.write("no input on stdin\n")
        sys.exit(1)

    # Minimal ad-hoc parser for the specific YC key format:
    # access_key:
    #   id: xxx
    #   key_id: xxx
    # secret: yyy
    access_key: dict = {}
    secret_access_key: str | None = None
    in_access_key = False

    for line in raw.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue

        # Top-level keys
        if not line.startswith(" ") and stripped.startswith("access_key:"):
            in_access_key = True
            continue
        if not line.startswith(" ") and stripped.startswith("secret:"):
            # secret: value
            _, _, value = stripped.partition(":")
            value = value.strip().strip('"').strip("'")
            secret_access_key = value
            continue

        # Nested under access_key
        if in_access_key and line.startswith(" "):
            # key: value
            key_part = stripped
            if ":" not in key_part:
                continue
            key, _, value = key_part.partition(":")
            key = key.strip()
            value = value.strip().strip('"').strip("'")
            access_key[key] = value

    if not access_key or secret_access_key is None:
        sys.stderr.write("invalid input: expected access_key and secret\n")
        sys.exit(1)

    access_key_id = access_key.get("key_id") or access_key.get("id")
    if not access_key_id:
        sys.stderr.write("invalid input: access_key.id or access_key.key_id is required\n")
        sys.exit(1)

    secdist = {
        "s3_credentials": {
            "access_key_id": access_key_id,
            "secret_access_key": secret_access_key,
        }
    }

    # Dump as a single JSON line
    json.dump(secdist, sys.stdout, separators=(",", ":"), ensure_ascii=False)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
