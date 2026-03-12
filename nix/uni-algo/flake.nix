{
  description = "Nix flake for uni-algo with constexpr patch";

  inputs = {
    nixpkgs.url = "nixpkgs";
    uniAlgoSrc = {
      url = "github:uni-algo/uni-algo/6c091fa266ac03a852128429af3b7481cf50a0ab";
      flake = false;
    };
  };

  outputs = {
    self,
    nixpkgs,
    uniAlgoSrc,
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
        f {inherit pkgs;});
  in {
    packages = forAllSystems ({pkgs, ...}: {
      default = import ./package.nix {
        inherit pkgs uniAlgoSrc;
      };
    });

    devShells = forAllSystems ({pkgs, ...}: let
      llvm = pkgs.llvmPackages_21;
    in {
      default = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [
          cmake
          ninja
          pkg-config
          llvm.clang
        ];
      };
    });
  };
}
