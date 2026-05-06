{pkgs}:
pkgs.callPackage "${pkgs.path}/pkgs/development/tools/analysis/include-what-you-use/default.nix" {
  llvmPackages = pkgs.llvmPackages_22;
  stdenv = pkgs.llvmPackages_22.stdenv;
}
