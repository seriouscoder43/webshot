{
  pkgs,
  src,
  toolchain,
}:
  toolchain.stdenv.mkDerivation {
    pname = "uni-algo";
    version = "1.2.0";
    inherit src;

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
