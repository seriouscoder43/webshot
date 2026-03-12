{
  description = "Nix flake for pgmigrate (PostgreSQL migrations tool)";

  inputs = {
    nixpkgs.url = "nixpkgs";
    flake-utils.url = "github:numtide/flake-utils";
    pgmigrateSrc = {
      url = "github:yandex/pgmigrate/76a0eec2cabadae6f3a66527d7e421ca2797bf90";
      flake = false;
    };
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    pgmigrateSrc,
    ...
  }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = import nixpkgs {
        inherit system;
        overlays = [(import ../overlay/boost_stacktrace_backtrace.nix)];
      };

      python = pkgs.python3;
      pyPkgs = pkgs.python3Packages;

      pgmigratePkg = import ./package.nix {
        inherit pgmigrateSrc pkgs;
      };
    in rec {
      packages = {
        default = pgmigratePkg;
      };

      # `nix flake check` will at least build the app.
      checks = {
        default = packages.default;
      };

      apps.default = {
        type = "app";
        program = "${packages.default}/bin/pgmigrate";
      };

      devShells.default = pkgs.mkShell {
        buildInputs = [
          python
          pyPkgs.sqlparse
          pyPkgs.psycopg2
          pyPkgs.pyyaml
          pkgs.postgresql
          pkgs.tox
        ];
      };
    });
}
