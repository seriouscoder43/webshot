{...}: {
  imports = let
    # Work around devenv eval caching: ensure changes in imported modules invalidate evaluation.
    import_files = [
      ./devenv/shared.nix
      ./devenv/packaging.nix
      ./devenv/webshot.nix
      ./webshotd/devenv_module.nix
    ];
    deps = builtins.concatStringsSep "" (map builtins.readFile import_files);
  in
    builtins.seq deps import_files;
}
