{
  description = "Nix flake for yandex-taxi-testsuite with PostgreSQL support";

  inputs = {
    nixpkgs.url = "nixpkgs";
    flake-utils.url = "github:numtide/flake-utils";
    pgmigrateSrc = {
      url = "github:yandex/pgmigrate/76a0eec2cabadae6f3a66527d7e421ca2797bf90";
      flake = false;
    };
    yandexTaxiTestsuiteSrc = {
      url = "github:yandex/yandex-taxi-testsuite/901c58e06b62f1438fcd4219f50cfe9b18b5bbf1";
      flake = false;
    };
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    pgmigrateSrc,
    yandexTaxiTestsuiteSrc,
    ...
  }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = import nixpkgs {
        inherit system;
        overlays = [(import ../overlay/boost_stacktrace_backtrace.nix)];
      };

      python = pkgs.python3;
      pyPkgs = pkgs.python3Packages;
      testsuitePkg = import ./package.nix {
        inherit pkgs pgmigrateSrc yandexTaxiTestsuiteSrc;
      };
    in rec {
      packages = {
        default = testsuitePkg;
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
