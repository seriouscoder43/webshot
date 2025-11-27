#!/usr/bin/env bash
set -euo pipefail

uv run conan install . \
  --profile:host=webshot-debug-san \
  --profile:host=conan/profiles/userver-clang-19 \
  --profile:build=webshot-debug-san \
  --profile:build=conan/profiles/userver-clang-19 \
  --output-folder=/tmp/build-webshot-san \
  --build=missing

cmake --preset configure-preset-clang-san
cmake --build --preset build-preset-clang-san
