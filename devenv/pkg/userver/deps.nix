{
  pkgs,
  srcs,
  python ? pkgs.python3,
}: let
  pyPkgs = python.pkgs;

  transliterate = pyPkgs.buildPythonPackage rec {
    name = "transliterate";

    pyproject = true;
    "build-system" = with pyPkgs; [setuptools wheel];

    src = srcs.transliterate;

    propagatedBuildInputs = with pyPkgs; [six];

    doCheck = false;
  };

  websocketsCompatible = pyPkgs.buildPythonPackage {
    name = "websockets";

    pyproject = true;
    "build-system" = with pyPkgs; [setuptools wheel];

    src = srcs.websockets;

    doCheck = false;
  };

  userverBuildPython = python.withPackages (_: [
    # Upstream userver helper requirements:
    # scripts/chaotic/requirements.txt
    pyPkgs.jinja2
    pyPkgs.pyyaml
    pyPkgs.pydantic
    transliterate
  ]);
in {
  inherit userverBuildPython;
  userverLibs = with pkgs; [
    python
    openssl
    jemalloc
    zlib
    boost183
    libbacktrace
    zstd
    yaml-cpp
    cryptopp
    fmt
    cctz
    re2
    abseil-cpp
    gbenchmark
    gtest
    libev
    libpq
    curl
    c-ares
    nghttp2
    pugixml
  ];
}
