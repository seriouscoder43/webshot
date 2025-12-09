{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";
    flake-utils.url = "github:numtide/flake-utils";
  };
  outputs = {
    self,
    nixpkgs,
    flake-utils,
    ...
  } @ inputs:
    flake-utils.lib.eachDefaultSystem (
      system: let
        system = "x86_64-linux";
        pkgs = import nixpkgs {inherit system;};
        toolchain = import ./nix/toolchain.nix {inherit pkgs;};
        buildDeps = import ./nix/common_deps.nix {inherit pkgs;};
      in {
        packages.webshot = toolchain.stdenv.mkDerivation {
          pname = "webshot";
          version = "0.1.0";
          src = ./.;
          nativeBuildInputs = buildDeps.native ++ [toolchain.cc];
          buildInputs = buildDeps.runtime;
          hardeningDisable = ["all"];
        };
        formatter = pkgs.alejandra;
      }
    );
}
