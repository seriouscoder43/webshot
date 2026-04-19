{pkgs}:
pkgs.stdenv.mkDerivation rec {
  pname = "stateforward-sml";
  version = "unstable-2026-04-02";

  src = pkgs.fetchFromGitHub {
    owner = "stateforward";
    repo = "sml.cpp";
    rev = "4a7109b5dd4aae40e78304e3ac03440ccc35031e";
    hash = "sha256-gRBOpIQwUvzKP+eUSMA74qu70JbL5BegB7FFq0GpZsI=";
  };

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
