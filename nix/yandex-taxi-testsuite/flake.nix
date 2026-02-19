{
  description = "Nix flake for yandex-taxi-testsuite with PostgreSQL support";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    ...
  }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = import nixpkgs {
        inherit system;
        overlays = [(import ../overlay/boost_stacktrace_backtrace.nix)];
      };

      python = pkgs.python3;
      pyPkgs = pkgs.python3Packages;

      yttsSrc = pkgs.fetchFromGitHub {
        owner = "yandex";
        repo = "yandex-taxi-testsuite";
        rev = "901c58e06b62f1438fcd4219f50cfe9b18b5bbf1";
        hash = "sha256-lQ6KFtx3HAKgVcci0WkmCPDdygj2z/JKDQmabhwl4OE=";
      };

      pgmigratePkg = import ../pgmigrate/package.nix {inherit pkgs;};
    in rec {
      packages = {
        default = pyPkgs.buildPythonPackage {
          pname = "yandex-taxi-testsuite";
          version = "0.3.9";

          src = yttsSrc;

          # Use the modern pyproject-based builder interface (Nixpkgs 25.11).
          pyproject = true;
          "build-system" = with pyPkgs; [setuptools wheel];

          # Old setup.py declares setup_requires=['pytest-runner'], which is
          # removed from Nixpkgs 25.11; drop it to avoid a missing-dep error.
          postPatch = ''
            substituteInPlace setup.py \
              --replace "    setup_requires=['pytest-runner']," ""
          '';

          # Runtime deps from setup.py + postgresql extra.
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
              websockets
            ])
            ++ [pgmigratePkg];

          # Tests exercise multiple external services; disable for now.
          doCheck = false;
        };
      };

      # `nix flake check` will at least build the package.
      checks.default = packages.default;

      devShells.default = pkgs.mkShell {
        buildInputs = [
          python
          pyPkgs.setuptools
          pyPkgs.wheel
          packages.default
        ];
      };
    });
}
