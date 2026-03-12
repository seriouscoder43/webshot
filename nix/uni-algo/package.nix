{
  pkgs,
  uniAlgoSrc,
}: let
  llvm = pkgs.llvmPackages_21;
  stdenv = llvm.stdenv;
in
  stdenv.mkDerivation {
    pname = "uni-algo";
    version = "1.2.0";
    src = uniAlgoSrc;

    patches = [../../patch/uni_algo_enable_constexpr.patch];

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
  }
