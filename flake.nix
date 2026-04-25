{
  outputs = {self}:
    (import ./devenv/flake_shim.nix).outputs {
      lockFile = ./devenv.lock;
      root = self.outPath;
    };
}
