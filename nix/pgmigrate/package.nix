{
  pkgs,
  pgmigrateSrc,
}: let
  pyPkgs = pkgs.python3Packages;
in
  pyPkgs.buildPythonApplication {
    pname = "pgmigrate";
    version = "1.0.10";

    # Use the modern pyproject-based builder interface.
    pyproject = true;
    "build-system" = with pyPkgs; [setuptools wheel];

    src = pgmigrateSrc;

    # Translate setup.py install_requires to Nix deps.
    # Upstream only requires `future` on Python 2; we target Python 3,
    # so it is intentionally omitted here to avoid version conflicts.
    propagatedBuildInputs = with pyPkgs; [
      sqlparse
      psycopg2
      pyyaml
    ];

    # Tests expect a running PostgreSQL instance; disable in build.
    doCheck = false;
  }
