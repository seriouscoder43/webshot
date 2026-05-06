{
  pkgs,
  inputs,
}:
inputs.seaweedfsNixpkgs.legacyPackages.${pkgs.system}.seaweedfs
