{
  pkgs,
  src,
  toolchain,
}:
toolchain.stdenv.mkDerivation {
  name = "uni-algo";
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
