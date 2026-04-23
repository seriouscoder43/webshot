{...}: {
  imports = let
    # Work around devenv eval caching: ensure changes in imported modules invalidate evaluation.
    import_files = [
      ./devenv/qa.nix
      ./devenv/outputs.nix
      ./devenv/tasks.nix
      ./webshotd/devenv_module.nix
    ];
    # Include non-module nix files that affect imported modules (but must not be imported themselves).
    deps_files =
      import_files
      ++ [
        ./devenv/ctx.nix
        ./devenv/drv.nix
        ./devenv/overlay/backtrace.nix
        ./devenv/pkgs/pgmigrate.nix
        ./devenv/pkgs/seaweedfs.nix
        ./devenv/pkgs/testsuite.nix
        ./devenv/pkgs/boost-sml.nix
        ./patch/boost_sml_disable_min_size.patch
        ./devenv/pkgs/include-what-you-use.nix
        ./devenv/pkgs/uni-algo.nix
        ./devenv/pkgs/userver.nix
        ./patch/userver_testsuite_no_venv.patch
        ./patch/userver_chaotic_no_venv.patch
        ./patch/userver_sql_no_venv.patch
        ./patch/userver_openssl_imported_targets.patch
        ./patch/userver_cctz_cmake_version.patch
        ./patch/userver_stdlib_cxx17_variant.patch
        ./devenv/pkgs/userver/deps.nix
        ./devenv/sets.nix
        ./devenv/srcs.nix
        ./devenv/toolchain.nix
      ];
    deps = builtins.concatStringsSep "" (map builtins.readFile deps_files);
  in
    builtins.seq deps import_files;
}
