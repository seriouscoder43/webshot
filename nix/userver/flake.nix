{
  description = "Parametrized userver builds (Release and Debug with addr+ub sanitizers)";

  inputs = {
    nixpkgs.url = "nixpkgs";
    userverSrc = {
      url = "github:userver-framework/userver/ba2ae1fefaebacd41647d6b3fcf5023546a67065";
      flake = false;
    };
  };

  outputs = {
    self,
    nixpkgs,
    userverSrc,
    ...
  }: let
    systems = ["x86_64-linux"];

    forAllSystems = f:
      nixpkgs.lib.genAttrs systems
      (system: let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [(import ../overlay/boost_stacktrace_backtrace.nix)];
        };
      in
        f {
          inherit pkgs;
        });
  in {
    packages = forAllSystems ({pkgs, ...}:
      import ./packages.nix {
        inherit pkgs userverSrc;
      });
  };
}
