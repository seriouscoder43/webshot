{
  pkgs,
  src,
}:
pkgs.stdenv.mkDerivation {
  name = "stateforward-sml";
  inherit src;

  nativeBuildInputs = with pkgs; [cmake];
  patches = [../../patch/boost_sml_disable_min_size.patch];

  cmakeFlags = [
    "-DSML_BUILD_BENCHMARKS=OFF"
    "-DSML_BUILD_EXAMPLES=OFF"
    "-DSML_BUILD_TESTS=OFF"
    "-DSML_USE_EXCEPTIONS=ON"
  ];

  meta = {
    description = "Header-only state machine library from the stateforward fork of sml";
    homepage = "https://github.com/stateforward/sml.cpp";
    license = pkgs.lib.licenses.boost;
    platforms = pkgs.lib.platforms.all;
  };
}
