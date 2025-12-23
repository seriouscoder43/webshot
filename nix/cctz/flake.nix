{
  description = "CCTZ packaged with Nix flakes";

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
        overlays = [(import ../overlays/boost-stacktrace-backtrace.nix)];
      };

      cctzSrc = pkgs.fetchFromGitHub {
        owner = "google";
        repo = "cctz";
        rev = "d2f2abda066d74c6e110b6be959d50bfb365917a";
        hash = "sha256-YixKEM3Q+IV2SS3/6o3J4lPVQ9SZI2ZZc23PgPlyYyI=";
      };
    in rec {
      packages = let
        cctz = pkgs.stdenv.mkDerivation {
          pname = "cctz";
          version = "unstable-d2f2abda";

          src = cctzSrc;

          nativeBuildInputs = [
            pkgs.cmake
          ];

          buildInputs = [];

          cmakeFlags = [
            "-DBUILD_TESTING=OFF"
            "-DBUILD_EXAMPLES=OFF"
            "-DBUILD_BENCHMARK=OFF"
            "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
          ];

          doCheck = false;

          meta = with pkgs.lib; {
            description = "C++ library for Civil Time and Time Zone (CCTZ)";
            homepage = "https://github.com/google/cctz";
            license = licenses.asl20;
            platforms = platforms.unix;
          };
        };
      in {
        inherit cctz;
        default = cctz;
      };

      # `nix flake check` at least builds the library.
      checks = {
        default = packages.default;
      };

      apps = let
        app = {
          type = "app";
          program = "${packages.default}/bin/time_tool";
        };
      in {
        default = app;
        time-tool = app;
      };

      devShells.default = pkgs.mkShell {
        nativeBuildInputs = [
          pkgs.cmake
          pkgs.pkg-config
        ];

        buildInputs = [
          pkgs.gtest
          pkgs.gmock
          pkgs.benchmark
        ];
      };
    });
}
