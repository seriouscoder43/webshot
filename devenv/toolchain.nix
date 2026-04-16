{pkgs}: let
  llvm = pkgs.llvmPackages_22;
in {
  stdenv = llvm.stdenv;
  cc = llvm.clang;
}
