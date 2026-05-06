{
  pkgs,
  pgmigrate,
  src,
}: let
  pyPkgs = pkgs.python3Packages;
in
  pyPkgs.buildPythonPackage {
    name = "yandex-taxi-testsuite";

    inherit src;

    pyproject = true;
    "build-system" = with pyPkgs; [setuptools wheel];

    postPatch = ''
      substituteInPlace setup.py \
        --replace "    setup_requires=['pytest-runner']," ""
    '';

    propagatedBuildInputs =
      (with pyPkgs; [
        packaging
        pyyaml
        aiohttp
        yarl
        py
        pytest-aiohttp
        pytest-asyncio
        pytest
        python-dateutil
        cached-property
        psycopg2
      ])
      ++ [pgmigrate];

    doCheck = false;
  }
