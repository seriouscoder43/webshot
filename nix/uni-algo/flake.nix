{
  description = "Nix flake for uni-algo with constexpr patch";

  outputs = {
    self,
    nixpkgs,
    ...
  }: let
    systems = ["x86_64-linux"];

    forAllSystems = f:
      nixpkgs.lib.genAttrs systems
      (system: let
        pkgs = import nixpkgs {inherit system;};
        llvm = pkgs.llvmPackages_21;
        stdenv = llvm.stdenv;
      in
        f {inherit pkgs llvm stdenv;});

    uniAlgoSrc = pkgs:
      pkgs.fetchFromGitHub {
        owner = "uni-algo";
        repo = "uni-algo";
        rev = "6c091fa266ac03a852128429af3b7481cf50a0ab";
        hash = "sha256-XYZs8Kx13Y+LZk5xuC3VfwZGDTOvmrNQNHCoZxOGS70=";
      };
  in {
    packages = forAllSystems ({
      pkgs,
      stdenv,
      llvm,
      ...
    }: {
      default = stdenv.mkDerivation {
        pname = "uni-algo";
        version = "1.2.0";
        src = uniAlgoSrc pkgs;

        patches = [../../patches/uni-algo_enable_constexpr.patch];

        nativeBuildInputs = with pkgs; [
          cmake
          ninja
          pkg-config
        ];

        cmakeFlags = [
          "-DUNI_ALGO_HEADER_ONLY=ON"
          "-DUNI_ALGO_INSTALL=ON"
        ];
        hardeningDisable = ["all"];
      };

      test = stdenv.mkDerivation {
        pname = "uni-algo-test";
        version = "0.1.0";
        src = uniAlgoSrc pkgs;

        nativeBuildInputs = with pkgs; [llvm.clang];
        hardeningDisable = ["all"];

        buildPhase = ''
          cat > main.cpp << 'EOF'
          int main() { return 0; }
          EOF
          echo "CXX is: $CXX"
          echo "Compile command (-###):"
          $CXX -### -std=c++20 -O1 -g main.cpp -o uni-algo-test 2>&1
          echo "Compile command (actual):"
          $CXX -std=c++20 -O1 -g main.cpp -o uni-algo-test
        '';

        installPhase = ''
          mkdir -p "$out/bin"
          cp uni-algo-test "$out/bin/"
        '';
      };
    });

    devShells = forAllSystems ({
      pkgs,
      llvm,
      ...
    }: {
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
