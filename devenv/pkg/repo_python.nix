{
  pkgs,
  s6Src,
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

  toolPackages = [
    # Repo build and test helpers still need the userver generator deps.
    pyPkgs.jinja2
    pyPkgs.pyyaml
    pyPkgs.pydantic
    transliterate

    # Repo-enabled local tests and helper flows.
    pyPkgs.minio
    pyPkgs.playwright
    pyPkgs.py
    pyPkgs.psycopg2
    pyPkgs.pytest
    pyPkgs.pytest-xdist
    pyPkgs.requests
    # userver testsuite currently requires websockets < 13.
    websocketsCompatible
    pyPkgs.zstd
  ];

  s6Runtime = pyPkgs.buildPythonPackage {
    name = "s6-runtime";

    pyproject = false;
    dontUnpack = true;

    src = s6Src;
    propagatedBuildInputs = [
      pyPkgs.minio
      pyPkgs.pyyaml
    ];

    installPhase = ''
      runHook preInstall

      package_dir="$out/${python.sitePackages}/s6"
      mkdir -p "$package_dir"
      cp "$src"/*.py "$package_dir/"

      runHook postInstall
    '';

    pythonImportsCheck = ["s6.runtime"];
  };
in {
  repoPy = python.withPackages (_: toolPackages ++ [s6Runtime]);
  repoToolPy = python.withPackages (_: toolPackages);
}
